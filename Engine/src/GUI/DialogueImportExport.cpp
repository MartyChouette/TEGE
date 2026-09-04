#include "Enjin/GUI/DialogueImportExport.h"
#include "Enjin/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace Enjin {
namespace GUI {

// ============================================================================
// YARN SPINNER IMPORT
// ============================================================================

std::vector<YarnSpinnerIO::YarnNodeBlock> YarnSpinnerIO::ParseBlocks(const std::string& text) {
    std::vector<YarnNodeBlock> blocks;
    std::istringstream stream(text);
    std::string line;
    u32 lineNum = 0;

    YarnNodeBlock current;
    bool inBody = false;

    while (std::getline(stream, line)) {
        ++lineNum;

        // Remove trailing \r (Windows line endings)
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line == "---") {
            // Start of body
            inBody = true;
            continue;
        }
        if (line == "===") {
            // End of node
            if (!current.title.empty()) {
                blocks.push_back(current);
            }
            current = {};
            inBody = false;
            continue;
        }

        if (!inBody) {
            // Header section
            auto colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string val = line.substr(colonPos + 1);
                // Trim whitespace
                while (!val.empty() && val.front() == ' ') val.erase(val.begin());
                while (!key.empty() && key.back() == ' ') key.pop_back();

                if (key == "title") {
                    current.title = val;
                    current.lineNumber = lineNum;
                } else if (key == "tags") {
                    current.tags = val;
                }
            }
        } else {
            // Body text
            if (!current.body.empty()) current.body += '\n';
            current.body += line;
        }
    }

    // Handle missing === at end of file
    if (!current.title.empty()) {
        blocks.push_back(current);
    }

    return blocks;
}

std::string YarnSpinnerIO::StripTags(const std::string& line) {
    // Remove inline tags like #tag1 #tag2
    auto pos = line.find('#');
    if (pos != std::string::npos) {
        std::string result = line.substr(0, pos);
        while (!result.empty() && result.back() == ' ') result.pop_back();
        return result;
    }
    return line;
}

void YarnSpinnerIO::ConvertBlockToNodes(const YarnNodeBlock& block, DialogueTreeData& tree,
                                         std::unordered_map<std::string, u32>& titleToNodeId,
                                         DialogueIOResult& result) {
    std::istringstream stream(block.body);
    std::string line;

    // Create an entry node for this Yarn node (maps to title)
    u32 entryNodeId = tree.AddNode(DialogueNodeType::Text);
    auto* entryNode = tree.GetNode(entryNodeId);
    if (!entryNode) return;

    titleToNodeId[block.title] = entryNodeId;
    entryNode->speakerName = "";
    entryNode->text = "";

    u32 prevNodeId = entryNodeId;
    bool firstLine = true;

    // Collect choice lines for grouping
    struct PendingChoice {
        std::string text;
        std::string jumpTarget; // If choice has inline jump
    };
    std::vector<PendingChoice> pendingChoices;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Trim leading whitespace
        std::string trimmed = line;
        while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) {
            trimmed.erase(trimmed.begin());
        }
        if (trimmed.empty()) continue;

        // Check for Yarn commands
        if (trimmed.substr(0, 2) == "<<") {
            // Command: <<jump NodeTitle>>, <<set $var to value>>, <<if $var ...>>
            auto endCmd = trimmed.find(">>");
            if (endCmd != std::string::npos) {
                std::string cmd = trimmed.substr(2, endCmd - 2);

                // Trim command
                while (!cmd.empty() && cmd.front() == ' ') cmd.erase(cmd.begin());
                while (!cmd.empty() && cmd.back() == ' ') cmd.pop_back();

                if (cmd.substr(0, 5) == "jump ") {
                    // <<jump NodeTitle>>
                    std::string target = cmd.substr(5);
                    while (!target.empty() && target.front() == ' ') target.erase(target.begin());

                    // Create a placeholder — will resolve after all blocks are parsed
                    u32 jumpNodeId = tree.AddNode(DialogueNodeType::Event);
                    auto* jumpNode = tree.GetNode(jumpNodeId);
                    if (jumpNode) {
                        jumpNode->eventName = "__jump:" + target;

                        // Link previous node to this
                        auto* prev = tree.GetNode(prevNodeId);
                        if (prev && prev->type != DialogueNodeType::Choice) {
                            prev->nextNodeId = jumpNodeId;
                        }
                        prevNodeId = jumpNodeId;
                    }
                    result.nodeCount++;
                } else if (cmd.substr(0, 4) == "set ") {
                    // <<set $var to value>> or <<set $var = value>>
                    std::string rest = cmd.substr(4);
                    // Find variable name
                    std::string varName;
                    std::string varValue;

                    auto toPos = rest.find(" to ");
                    auto eqPos = rest.find(" = ");
                    if (toPos != std::string::npos) {
                        varName = rest.substr(0, toPos);
                        varValue = rest.substr(toPos + 4);
                    } else if (eqPos != std::string::npos) {
                        varName = rest.substr(0, eqPos);
                        varValue = rest.substr(eqPos + 3);
                    }

                    // Remove $ prefix
                    if (!varName.empty() && varName.front() == '$') varName = varName.substr(1);
                    while (!varName.empty() && varName.front() == ' ') varName.erase(varName.begin());
                    while (!varValue.empty() && varValue.front() == ' ') varValue.erase(varValue.begin());

                    if (!varName.empty()) {
                        u32 setNodeId = tree.AddNode(DialogueNodeType::SetVariable);
                        auto* setNode = tree.GetNode(setNodeId);
                        if (setNode) {
                            setNode->variableName = varName;
                            setNode->variableValue = varValue;
                            auto* prev = tree.GetNode(prevNodeId);
                            if (prev && prev->type != DialogueNodeType::Choice) {
                                prev->nextNodeId = setNodeId;
                            }
                            prevNodeId = setNodeId;
                        }
                        result.nodeCount++;
                    }
                } else if (cmd.substr(0, 3) == "if ") {
                    // <<if $var == value>> — create condition node
                    std::string expr = cmd.substr(3);
                    std::string varName;
                    DialogueCondition::Op op = DialogueCondition::Op::Equals;
                    std::string value;

                    // Parse: $var == value, $var != value, $var > value, $var < value
                    auto eqEq = expr.find("==");
                    auto neq = expr.find("!=");
                    auto gt = expr.find(">");
                    auto lt = expr.find("<");

                    usize opPos = std::string::npos;
                    usize opLen = 0;

                    if (neq != std::string::npos) { opPos = neq; opLen = 2; op = DialogueCondition::Op::NotEquals; }
                    else if (eqEq != std::string::npos) { opPos = eqEq; opLen = 2; op = DialogueCondition::Op::Equals; }
                    else if (gt != std::string::npos) { opPos = gt; opLen = 1; op = DialogueCondition::Op::GreaterThan; }
                    else if (lt != std::string::npos) { opPos = lt; opLen = 1; op = DialogueCondition::Op::LessThan; }

                    if (opPos != std::string::npos) {
                        varName = expr.substr(0, opPos);
                        value = expr.substr(opPos + opLen);
                        if (!varName.empty() && varName.front() == '$') varName = varName.substr(1);
                        while (!varName.empty() && varName.back() == ' ') varName.pop_back();
                        while (!varName.empty() && varName.front() == ' ') varName.erase(varName.begin());
                        while (!value.empty() && value.front() == ' ') value.erase(value.begin());
                        while (!value.empty() && value.back() == ' ') value.pop_back();

                        u32 condNodeId = tree.AddNode(DialogueNodeType::Condition);
                        auto* condNode = tree.GetNode(condNodeId);
                        if (condNode) {
                            condNode->condition.variable = varName;
                            condNode->condition.op = op;
                            condNode->condition.value = value;
                            auto* prev = tree.GetNode(prevNodeId);
                            if (prev && prev->type != DialogueNodeType::Choice) {
                                prev->nextNodeId = condNodeId;
                            }
                            prevNodeId = condNodeId;
                        }
                        result.nodeCount++;
                    }
                }
                // <<endif>>, <<else>> — skip (simplified parsing)
            }
            continue;
        }

        // Check for choice line: -> Choice text
        if (trimmed.substr(0, 3) == "-> ") {
            std::string choiceText = trimmed.substr(3);
            choiceText = StripTags(choiceText);

            PendingChoice pc;
            pc.text = choiceText;
            // Check for inline jump: -> Choice text <<jump Target>>
            auto jumpPos = choiceText.find("<<jump ");
            if (jumpPos != std::string::npos) {
                auto endPos = choiceText.find(">>", jumpPos);
                if (endPos != std::string::npos) {
                    pc.jumpTarget = choiceText.substr(jumpPos + 7, endPos - jumpPos - 7);
                    pc.text = choiceText.substr(0, jumpPos);
                    while (!pc.text.empty() && pc.text.back() == ' ') pc.text.pop_back();
                }
            }
            pendingChoices.push_back(pc);
            continue;
        }

        // Flush any pending choices before the next text line
        if (!pendingChoices.empty()) {
            u32 choiceNodeId = tree.AddNode(DialogueNodeType::Choice);
            auto* choiceNode = tree.GetNode(choiceNodeId);
            if (choiceNode) {
                for (const auto& pc : pendingChoices) {
                    DialogueChoice dc;
                    dc.text = pc.text;
                    dc.targetNodeId = 0; // Will resolve jumps later
                    if (!pc.jumpTarget.empty()) {
                        // Store jump target name in text for later resolution
                        dc.text = pc.text;
                        // We'll use a special prefix to resolve later
                    }
                    choiceNode->choices.push_back(dc);
                }
                auto* prev = tree.GetNode(prevNodeId);
                if (prev && prev->type != DialogueNodeType::Choice) {
                    prev->nextNodeId = choiceNodeId;
                }
                prevNodeId = choiceNodeId;
            }
            result.nodeCount++;
            pendingChoices.clear();
        }

        // Regular dialogue line: Speaker: Text or just Text
        std::string speaker;
        std::string dialogueText;

        auto colonPos = trimmed.find(": ");
        if (colonPos != std::string::npos && colonPos < 30) {
            // Looks like "Speaker: dialogue text"
            speaker = trimmed.substr(0, colonPos);
            dialogueText = trimmed.substr(colonPos + 2);
        } else {
            dialogueText = StripTags(trimmed);
        }

        if (firstLine && !dialogueText.empty()) {
            // Use the entry node for the first line
            entryNode->speakerName = speaker;
            entryNode->text = dialogueText;
            firstLine = false;
        } else if (!dialogueText.empty()) {
            u32 textNodeId = tree.AddNode(DialogueNodeType::Text);
            auto* textNode = tree.GetNode(textNodeId);
            if (textNode) {
                textNode->speakerName = speaker;
                textNode->text = dialogueText;

                auto* prev = tree.GetNode(prevNodeId);
                if (prev && prev->type != DialogueNodeType::Choice) {
                    prev->nextNodeId = textNodeId;
                }
                prevNodeId = textNodeId;
            }
            result.nodeCount++;
        }
    }

    // Flush remaining choices
    if (!pendingChoices.empty()) {
        u32 choiceNodeId = tree.AddNode(DialogueNodeType::Choice);
        auto* choiceNode = tree.GetNode(choiceNodeId);
        if (choiceNode) {
            for (const auto& pc : pendingChoices) {
                DialogueChoice dc;
                dc.text = pc.text;
                dc.targetNodeId = 0;
                choiceNode->choices.push_back(dc);
            }
            auto* prev = tree.GetNode(prevNodeId);
            if (prev && prev->type != DialogueNodeType::Choice) {
                prev->nextNodeId = choiceNodeId;
            }
        }
        result.nodeCount++;
    }

    // Add End node if last node has no next
    auto* lastNode = tree.GetNode(prevNodeId);
    if (lastNode && lastNode->nextNodeId == 0 && lastNode->type != DialogueNodeType::Choice &&
        lastNode->type != DialogueNodeType::End) {
        u32 endNodeId = tree.AddNode(DialogueNodeType::End);
        lastNode->nextNodeId = endNodeId;
    }
}

DialogueIOResult YarnSpinnerIO::ImportFromString(const std::string& yarnText, DialogueTreeData& outTree) {
    DialogueIOResult result;

    outTree = {};
    outTree.treeName = "Imported Yarn";

    auto blocks = ParseBlocks(yarnText);
    if (blocks.empty()) {
        result.errorMessage = "No Yarn nodes found in file";
        return result;
    }

    // Create root node
    u32 rootId = outTree.AddNode(DialogueNodeType::Root);
    outTree.rootNodeId = rootId;

    std::unordered_map<std::string, u32> titleToNodeId;

    // Convert each block
    for (const auto& block : blocks) {
        ConvertBlockToNodes(block, outTree, titleToNodeId, result);
    }

    // Resolve jump references
    for (auto& node : outTree.nodes) {
        if (node.type == DialogueNodeType::Event && node.eventName.substr(0, 7) == "__jump:") {
            std::string target = node.eventName.substr(7);
            auto it = titleToNodeId.find(target);
            if (it != titleToNodeId.end()) {
                // Convert jump to a direct link
                node.type = DialogueNodeType::Text;
                node.text = "";
                node.nextNodeId = it->second;
                node.eventName.clear();
            } else {
                result.warnings.push_back("Unresolved jump target: " + target);
            }
        }
    }

    // Link root to first block's entry node
    if (!blocks.empty()) {
        auto it = titleToNodeId.find(blocks[0].title);
        if (it != titleToNodeId.end()) {
            auto* root = outTree.GetNode(rootId);
            if (root) root->nextNodeId = it->second;
        }
    }

    // Auto-layout nodes
    f32 x = 100.0f, y = 0.0f;
    for (auto& node : outTree.nodes) {
        node.editorPosition = Math::Vector2(x, y);
        y += 80.0f;
        if (y > 1000.0f) { y = 0.0f; x += 300.0f; }
    }

    result.success = true;
    result.nodeCount = static_cast<u32>(outTree.nodes.size());
    result.linkCount = 0;
    for (const auto& node : outTree.nodes) {
        if (node.nextNodeId != 0) result.linkCount++;
        if (node.trueNodeId != 0) result.linkCount++;
        if (node.falseNodeId != 0) result.linkCount++;
        result.linkCount += static_cast<u32>(node.choices.size());
    }

    ENJIN_LOG_INFO(Editor, "Imported Yarn: %u nodes, %u links from %zu blocks",
                   result.nodeCount, result.linkCount, blocks.size());
    return result;
}

DialogueIOResult YarnSpinnerIO::Import(const std::string& filepath, DialogueTreeData& outTree) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        DialogueIOResult result;
        result.errorMessage = "Failed to open file: " + filepath;
        return result;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto result = ImportFromString(content, outTree);

    if (result.success) {
        outTree.treeName = std::filesystem::path(filepath).stem().string();
    }
    return result;
}

// --- Yarn Export ---

std::string YarnSpinnerIO::ExportToString(const DialogueTreeData& tree) {
    std::ostringstream out;

    // Group nodes into "pages" — each root-reachable subtree becomes a Yarn node
    // For simplicity, export the entire tree as a single Yarn node
    out << "title: " << (tree.treeName.empty() ? "Start" : tree.treeName) << "\n";
    out << "tags: \n";
    out << "---\n";

    // Walk the tree linearly from root
    std::unordered_set<u32> visited;
    std::function<void(u32)> walk = [&](u32 nodeId) {
        if (nodeId == 0 || visited.count(nodeId)) return;
        visited.insert(nodeId);

        auto* node = const_cast<DialogueTreeData&>(tree).GetNode(nodeId);
        if (!node) return;

        switch (node->type) {
            case DialogueNodeType::Root:
                walk(node->nextNodeId);
                break;
            case DialogueNodeType::Text:
                if (!node->speakerName.empty()) {
                    out << node->speakerName << ": " << node->text << "\n";
                } else if (!node->text.empty()) {
                    out << node->text << "\n";
                }
                walk(node->nextNodeId);
                break;
            case DialogueNodeType::Choice:
                if (!node->text.empty()) {
                    out << node->text << "\n";
                }
                for (const auto& choice : node->choices) {
                    out << "-> " << choice.text << "\n";
                }
                break;
            case DialogueNodeType::SetVariable:
                out << "<<set $" << node->variableName << " to " << node->variableValue << ">>\n";
                walk(node->nextNodeId);
                break;
            case DialogueNodeType::Condition: {
                std::string opStr = "==";
                switch (node->condition.op) {
                    case DialogueCondition::Op::Equals:      opStr = "=="; break;
                    case DialogueCondition::Op::NotEquals:    opStr = "!="; break;
                    case DialogueCondition::Op::GreaterThan:  opStr = ">"; break;
                    case DialogueCondition::Op::LessThan:     opStr = "<"; break;
                }
                out << "<<if $" << node->condition.variable << " " << opStr << " " << node->condition.value << ">>\n";
                walk(node->trueNodeId);
                out << "<<else>>\n";
                walk(node->falseNodeId);
                out << "<<endif>>\n";
                break;
            }
            case DialogueNodeType::Event:
                out << "<<" << node->eventName << ">>\n";
                walk(node->nextNodeId);
                break;
            // These three were in the enum and in neither exporter's switch, so
            // they wrote nothing AND never advanced — the rest of the branch
            // after one of them was dropped from the file. Written as Yarn
            // commands, the same shape the Event case above already uses.
            case DialogueNodeType::QuestAction: {
                const char* verb = "start";
                switch (node->questAction) {
                    case DialogueNode::QuestActionType::StartQuest:       verb = "start"; break;
                    case DialogueNode::QuestActionType::CompleteObjective: verb = "complete"; break;
                    case DialogueNode::QuestActionType::FailQuest:         verb = "fail"; break;
                }
                out << "<<quest " << verb << " " << node->questId;
                if (node->questAction == DialogueNode::QuestActionType::CompleteObjective)
                    out << " " << node->questObjectiveIndex;
                out << ">>\n";
                walk(node->nextNodeId);
                break;
            }
            case DialogueNodeType::PlayCinematic:
                out << "<<cinematic " << node->cinematicEntityName << ">>\n";
                walk(node->nextNodeId);
                break;
            case DialogueNodeType::SetGameFlag:
                out << "<<flag " << node->gameFlagKey << " " << node->gameFlagValue << ">>\n";
                walk(node->nextNodeId);
                break;
            case DialogueNodeType::End:
                break;
        }
    };

    walk(tree.rootNodeId);

    out << "===\n";
    return out.str();
}

DialogueIOResult YarnSpinnerIO::Export(const std::string& filepath, const DialogueTreeData& tree) {
    DialogueIOResult result;
    std::string content = ExportToString(tree);

    std::ofstream file(filepath);
    if (!file.is_open()) {
        result.errorMessage = "Failed to write file: " + filepath;
        return result;
    }

    file << content;
    result.success = true;
    result.nodeCount = static_cast<u32>(tree.nodes.size());
    ENJIN_LOG_INFO(Editor, "Exported Yarn: %s (%u nodes)", filepath.c_str(), result.nodeCount);
    return result;
}

// ============================================================================
// TWINE IMPORT
// ============================================================================

bool TwineIO::IsHTML(const std::string& text) {
    return text.find("<tw-storydata") != std::string::npos ||
           text.find("<!DOCTYPE html") != std::string::npos ||
           text.find("<html") != std::string::npos;
}

std::vector<TwineIO::TwinePassage> TwineIO::ParseHTML(const std::string& html) {
    std::vector<TwinePassage> passages;

    // Find all <tw-passagedata> elements
    std::string searchTag = "<tw-passagedata";
    usize pos = 0;

    while ((pos = html.find(searchTag, pos)) != std::string::npos) {
        TwinePassage passage;

        // Extract name attribute
        auto namePos = html.find("name=\"", pos);
        if (namePos != std::string::npos && namePos < html.find(">", pos)) {
            auto nameEnd = html.find("\"", namePos + 6);
            if (nameEnd != std::string::npos) {
                passage.name = html.substr(namePos + 6, nameEnd - namePos - 6);
            }
        }

        // Extract tags attribute
        auto tagsPos = html.find("tags=\"", pos);
        if (tagsPos != std::string::npos && tagsPos < html.find(">", pos)) {
            auto tagsEnd = html.find("\"", tagsPos + 6);
            if (tagsEnd != std::string::npos) {
                passage.tags = html.substr(tagsPos + 6, tagsEnd - tagsPos - 6);
            }
        }

        // Extract body (between > and </tw-passagedata>)
        auto bodyStart = html.find(">", pos);
        if (bodyStart != std::string::npos) {
            ++bodyStart;
            auto bodyEnd = html.find("</tw-passagedata>", bodyStart);
            if (bodyEnd != std::string::npos) {
                passage.body = html.substr(bodyStart, bodyEnd - bodyStart);

                // Unescape HTML entities
                std::string& b = passage.body;
                auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
                    usize p = 0;
                    while ((p = s.find(from, p)) != std::string::npos) {
                        s.replace(p, from.length(), to);
                        p += to.length();
                    }
                };
                replaceAll(b, "&amp;", "&");
                replaceAll(b, "&lt;", "<");
                replaceAll(b, "&gt;", ">");
                replaceAll(b, "&quot;", "\"");
                replaceAll(b, "&#39;", "'");

                passage.links = ExtractLinks(passage.body);
                pos = bodyEnd;
            }
        }

        if (!passage.name.empty()) {
            passages.push_back(passage);
        }
        ++pos;
    }

    return passages;
}

std::vector<TwineIO::TwinePassage> TwineIO::ParseTwee(const std::string& twee) {
    std::vector<TwinePassage> passages;
    std::istringstream stream(twee);
    std::string line;

    TwinePassage current;
    bool inPassage = false;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Passage header: :: PassageName [tags]
        if (line.size() >= 3 && line[0] == ':' && line[1] == ':' && line[2] == ' ') {
            // Save previous passage
            if (inPassage && !current.name.empty()) {
                current.links = ExtractLinks(current.body);
                passages.push_back(current);
            }

            current = {};
            inPassage = true;

            std::string header = line.substr(3);

            // Extract tags [tag1 tag2]
            auto tagStart = header.find('[');
            if (tagStart != std::string::npos) {
                auto tagEnd = header.find(']', tagStart);
                if (tagEnd != std::string::npos) {
                    current.tags = header.substr(tagStart + 1, tagEnd - tagStart - 1);
                    header = header.substr(0, tagStart);
                }
            }

            // Trim passage name
            while (!header.empty() && header.back() == ' ') header.pop_back();
            current.name = header;
        } else if (inPassage) {
            if (!current.body.empty()) current.body += '\n';
            current.body += line;
        }
    }

    // Save last passage
    if (inPassage && !current.name.empty()) {
        current.links = ExtractLinks(current.body);
        passages.push_back(current);
    }

    return passages;
}

std::vector<std::pair<std::string, std::string>> TwineIO::ExtractLinks(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> links;

    usize pos = 0;
    while ((pos = text.find("[[", pos)) != std::string::npos) {
        auto end = text.find("]]", pos);
        if (end == std::string::npos) break;

        std::string inner = text.substr(pos + 2, end - pos - 2);

        std::string display, target;

        // [[text->target]] format
        auto arrowPos = inner.find("->");
        if (arrowPos != std::string::npos) {
            display = inner.substr(0, arrowPos);
            target = inner.substr(arrowPos + 2);
        }
        // [[text|target]] format
        else {
            auto pipePos = inner.find('|');
            if (pipePos != std::string::npos) {
                display = inner.substr(0, pipePos);
                target = inner.substr(pipePos + 1);
            } else {
                // [[target]] format (text = target)
                display = inner;
                target = inner;
            }
        }

        // Trim whitespace
        while (!display.empty() && display.front() == ' ') display.erase(display.begin());
        while (!display.empty() && display.back() == ' ') display.pop_back();
        while (!target.empty() && target.front() == ' ') target.erase(target.begin());
        while (!target.empty() && target.back() == ' ') target.pop_back();

        if (!target.empty()) {
            links.emplace_back(display, target);
        }

        pos = end + 2;
    }

    return links;
}

std::string TwineIO::StripLinks(const std::string& text) {
    std::string result;
    usize pos = 0;

    while (pos < text.size()) {
        auto linkStart = text.find("[[", pos);
        if (linkStart == std::string::npos) {
            result += text.substr(pos);
            break;
        }
        result += text.substr(pos, linkStart - pos);

        auto linkEnd = text.find("]]", linkStart);
        if (linkEnd == std::string::npos) {
            result += text.substr(linkStart);
            break;
        }
        pos = linkEnd + 2;
    }

    // Trim trailing whitespace/newlines
    while (!result.empty() && (result.back() == ' ' || result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}

bool TwineIO::ParseSetMacro(const std::string& line, std::string& varName, std::string& varValue) {
    // Harlowe: (set: $var to value) / SugarCube: <<set $var to value>>
    std::string trimmed = line;
    bool isSugar = trimmed.find("<<set ") != std::string::npos;
    bool isHarlowe = trimmed.find("(set:") != std::string::npos;

    if (!isSugar && !isHarlowe) return false;

    std::string content;
    if (isSugar) {
        auto start = trimmed.find("<<set ");
        auto end = trimmed.find(">>", start);
        if (start == std::string::npos || end == std::string::npos) return false;
        content = trimmed.substr(start + 6, end - start - 6);
    } else {
        auto start = trimmed.find("(set:");
        auto end = trimmed.find(")", start);
        if (start == std::string::npos || end == std::string::npos) return false;
        content = trimmed.substr(start + 5, end - start - 5);
    }

    // Parse "$var to value"
    auto toPos = content.find(" to ");
    if (toPos == std::string::npos) return false;

    varName = content.substr(0, toPos);
    varValue = content.substr(toPos + 4);
    if (!varName.empty() && varName.front() == '$') varName = varName.substr(1);
    while (!varName.empty() && varName.front() == ' ') varName.erase(varName.begin());
    while (!varValue.empty() && varValue.front() == ' ') varValue.erase(varValue.begin());
    while (!varValue.empty() && varValue.back() == ' ') varValue.pop_back();

    return !varName.empty();
}

bool TwineIO::ParseIfMacro(const std::string& line, std::string& varName,
                            DialogueCondition::Op& op, std::string& value) {
    // Harlowe: (if: $var is value) / SugarCube: <<if $var == value>>
    std::string content;
    if (line.find("<<if ") != std::string::npos) {
        auto start = line.find("<<if ");
        auto end = line.find(">>", start);
        if (end == std::string::npos) return false;
        content = line.substr(start + 5, end - start - 5);
    } else if (line.find("(if:") != std::string::npos) {
        auto start = line.find("(if:");
        auto end = line.find(")", start);
        if (end == std::string::npos) return false;
        content = line.substr(start + 4, end - start - 4);
    } else {
        return false;
    }

    while (!content.empty() && content.front() == ' ') content.erase(content.begin());

    // Parse operator
    usize opPos = std::string::npos;
    usize opLen = 0;

    auto isPos = content.find(" is ");
    auto eqPos = content.find("==");
    auto nePos = content.find("!=");

    if (nePos != std::string::npos) { opPos = nePos; opLen = 2; op = DialogueCondition::Op::NotEquals; }
    else if (eqPos != std::string::npos) { opPos = eqPos; opLen = 2; op = DialogueCondition::Op::Equals; }
    else if (isPos != std::string::npos) { opPos = isPos; opLen = 4; op = DialogueCondition::Op::Equals; }

    if (opPos == std::string::npos) return false;

    varName = content.substr(0, opPos);
    value = content.substr(opPos + opLen);
    if (!varName.empty() && varName.front() == '$') varName = varName.substr(1);
    while (!varName.empty() && varName.back() == ' ') varName.pop_back();
    while (!varName.empty() && varName.front() == ' ') varName.erase(varName.begin());
    while (!value.empty() && value.front() == ' ') value.erase(value.begin());
    while (!value.empty() && value.back() == ' ') value.pop_back();

    return !varName.empty();
}

void TwineIO::ConvertPassagesToTree(const std::vector<TwinePassage>& passages,
                                     DialogueTreeData& tree, DialogueIOResult& result) {
    // Create root
    u32 rootId = tree.AddNode(DialogueNodeType::Root);
    tree.rootNodeId = rootId;

    // Map passage name -> node ID
    std::unordered_map<std::string, u32> passageToNodeId;

    // First pass: create a text node for each passage
    for (const auto& passage : passages) {
        std::string body = StripLinks(passage.body);

        // Check for macros in the body
        std::istringstream stream(passage.body);
        std::string line;
        u32 firstNodeId = 0;
        u32 prevNodeId = 0;

        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::string trimmed = line;
            while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
                trimmed.erase(trimmed.begin());
            if (trimmed.empty()) continue;

            // Check for set macro
            std::string varName, varValue;
            if (ParseSetMacro(trimmed, varName, varValue)) {
                u32 setId = tree.AddNode(DialogueNodeType::SetVariable);
                auto* setNode = tree.GetNode(setId);
                if (setNode) {
                    setNode->variableName = varName;
                    setNode->variableValue = varValue;
                    if (prevNodeId) {
                        auto* prev = tree.GetNode(prevNodeId);
                        if (prev && prev->type != DialogueNodeType::Choice) prev->nextNodeId = setId;
                    }
                    if (!firstNodeId) firstNodeId = setId;
                    prevNodeId = setId;
                }
                result.nodeCount++;
                continue;
            }

            // Check for if macro
            DialogueCondition::Op op;
            std::string condVar, condVal;
            if (ParseIfMacro(trimmed, condVar, op, condVal)) {
                u32 condId = tree.AddNode(DialogueNodeType::Condition);
                auto* condNode = tree.GetNode(condId);
                if (condNode) {
                    condNode->condition.variable = condVar;
                    condNode->condition.op = op;
                    condNode->condition.value = condVal;
                    if (prevNodeId) {
                        auto* prev = tree.GetNode(prevNodeId);
                        if (prev && prev->type != DialogueNodeType::Choice) prev->nextNodeId = condId;
                    }
                    if (!firstNodeId) firstNodeId = condId;
                    prevNodeId = condId;
                }
                result.nodeCount++;
                continue;
            }

            // Skip links and control macros
            if (trimmed.find("[[") != std::string::npos) continue;
            if (trimmed.find("<<") != std::string::npos) continue;
            if (trimmed.find("(if:") != std::string::npos) continue;

            // Regular text line
            std::string cleanText = StripLinks(trimmed);
            if (cleanText.empty()) continue;

            u32 textId = tree.AddNode(DialogueNodeType::Text);
            auto* textNode = tree.GetNode(textId);
            if (textNode) {
                textNode->text = cleanText;
                if (prevNodeId) {
                    auto* prev = tree.GetNode(prevNodeId);
                    if (prev && prev->type != DialogueNodeType::Choice) prev->nextNodeId = textId;
                }
                if (!firstNodeId) firstNodeId = textId;
                prevNodeId = textId;
            }
            result.nodeCount++;
        }

        // If passage has links, create a choice node
        if (!passage.links.empty() && passage.links.size() > 1) {
            u32 choiceId = tree.AddNode(DialogueNodeType::Choice);
            auto* choiceNode = tree.GetNode(choiceId);
            if (choiceNode) {
                for (const auto& [display, target] : passage.links) {
                    DialogueChoice dc;
                    dc.text = display;
                    dc.targetNodeId = 0; // Resolve later
                    choiceNode->choices.push_back(dc);
                }
                if (prevNodeId) {
                    auto* prev = tree.GetNode(prevNodeId);
                    if (prev && prev->type != DialogueNodeType::Choice) prev->nextNodeId = choiceId;
                }
                if (!firstNodeId) firstNodeId = choiceId;
                prevNodeId = choiceId;
            }
            result.nodeCount++;
        }

        if (firstNodeId) {
            passageToNodeId[passage.name] = firstNodeId;
        }
    }

    // Link root to first passage ("Start" or first passage)
    auto startIt = passageToNodeId.find("Start");
    if (startIt == passageToNodeId.end() && !passages.empty()) {
        startIt = passageToNodeId.find(passages[0].name);
    }
    if (startIt != passageToNodeId.end()) {
        auto* root = tree.GetNode(rootId);
        if (root) root->nextNodeId = startIt->second;
    }

    // Resolve passage links in choice nodes
    for (usize pi = 0; pi < passages.size(); ++pi) {
        const auto& passage = passages[pi];
        if (passage.links.size() == 1) {
            // Single link: connect the last node to the target
            auto nodeIt = passageToNodeId.find(passage.name);
            if (nodeIt != passageToNodeId.end()) {
                // Find the last node in this passage's chain
                u32 lastId = nodeIt->second;
                auto* lastNode = tree.GetNode(lastId);
                while (lastNode && lastNode->nextNodeId != 0) {
                    lastId = lastNode->nextNodeId;
                    lastNode = tree.GetNode(lastId);
                }
                if (lastNode && lastNode->type != DialogueNodeType::Choice) {
                    auto targetIt = passageToNodeId.find(passage.links[0].second);
                    if (targetIt != passageToNodeId.end()) {
                        lastNode->nextNodeId = targetIt->second;
                        result.linkCount++;
                    }
                }
            }
        } else {
            // Multiple links: resolve choice targets
            auto nodeIt = passageToNodeId.find(passage.name);
            if (nodeIt == passageToNodeId.end()) continue;

            // Find the choice node at the end of this passage's chain
            u32 lastId = nodeIt->second;
            auto* lastNode = tree.GetNode(lastId);
            while (lastNode && lastNode->nextNodeId != 0 && lastNode->type != DialogueNodeType::Choice) {
                lastId = lastNode->nextNodeId;
                lastNode = tree.GetNode(lastId);
            }

            if (lastNode && lastNode->type == DialogueNodeType::Choice) {
                for (usize ci = 0; ci < passage.links.size() && ci < lastNode->choices.size(); ++ci) {
                    auto targetIt = passageToNodeId.find(passage.links[ci].second);
                    if (targetIt != passageToNodeId.end()) {
                        lastNode->choices[ci].targetNodeId = targetIt->second;
                        result.linkCount++;
                    } else {
                        result.warnings.push_back("Unresolved link target: " + passage.links[ci].second);
                    }
                }
            }
        }
    }

    // Auto-layout
    f32 x = 100.0f, y = 0.0f;
    for (auto& node : tree.nodes) {
        node.editorPosition = Math::Vector2(x, y);
        y += 80.0f;
        if (y > 1000.0f) { y = 0.0f; x += 300.0f; }
    }
}

DialogueIOResult TwineIO::ImportFromString(const std::string& text, DialogueTreeData& outTree) {
    DialogueIOResult result;
    outTree = {};
    outTree.treeName = "Imported Twine";

    std::vector<TwinePassage> passages;
    if (IsHTML(text)) {
        passages = ParseHTML(text);
    } else {
        passages = ParseTwee(text);
    }

    if (passages.empty()) {
        result.errorMessage = "No passages found in file";
        return result;
    }

    ConvertPassagesToTree(passages, outTree, result);

    result.success = true;
    result.nodeCount = static_cast<u32>(outTree.nodes.size());

    ENJIN_LOG_INFO(Editor, "Imported Twine: %u nodes, %u links from %zu passages",
                   result.nodeCount, result.linkCount, passages.size());
    return result;
}

DialogueIOResult TwineIO::Import(const std::string& filepath, DialogueTreeData& outTree) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        DialogueIOResult result;
        result.errorMessage = "Failed to open file: " + filepath;
        return result;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto result = ImportFromString(content, outTree);

    if (result.success) {
        outTree.treeName = std::filesystem::path(filepath).stem().string();
    }
    return result;
}

// --- Twine Export (Twee 3 format) ---

std::string TwineIO::ExportToString(const DialogueTreeData& tree) {
    std::ostringstream out;

    // Twee 3 header
    out << ":: StoryTitle\n";
    out << (tree.treeName.empty() ? "Exported Dialogue" : tree.treeName) << "\n\n";

    out << ":: StoryData\n";
    out << "{\n";
    out << "  \"ifid\": \"00000000-0000-0000-0000-000000000000\",\n";
    out << "  \"format\": \"Harlowe\",\n";
    out << "  \"format-version\": \"3.3.7\"\n";
    out << "}\n\n";

    // Walk the tree and create passages
    // Each text node that has no incoming link from a choice becomes a passage start
    std::unordered_set<u32> visited;
    u32 passageNum = 0;

    std::function<void(u32, const std::string&)> walkPassage = [&](u32 nodeId, const std::string& passageName) {
        if (nodeId == 0 || visited.count(nodeId)) return;

        out << ":: " << passageName << "\n";

        u32 current = nodeId;
        while (current != 0 && !visited.count(current)) {
            visited.insert(current);
            auto* node = const_cast<DialogueTreeData&>(tree).GetNode(current);
            if (!node) break;

            switch (node->type) {
                case DialogueNodeType::Root:
                    current = node->nextNodeId;
                    continue;
                case DialogueNodeType::Text:
                    if (!node->speakerName.empty()) {
                        out << "**" << node->speakerName << ":** " << node->text << "\n";
                    } else if (!node->text.empty()) {
                        out << node->text << "\n";
                    }
                    current = node->nextNodeId;
                    break;
                case DialogueNodeType::Choice:
                    if (!node->text.empty()) {
                        out << node->text << "\n";
                    }
                    for (usize i = 0; i < node->choices.size(); ++i) {
                        const auto& choice = node->choices[i];
                        std::string targetName = "Choice" + std::to_string(passageNum) + "_" + std::to_string(i);
                        out << "[[" << choice.text << "->" << targetName << "]]\n";
                        // Queue the choice target as a new passage
                        if (choice.targetNodeId != 0 && !visited.count(choice.targetNodeId)) {
                            walkPassage(choice.targetNodeId, targetName);
                        }
                    }
                    current = 0;
                    break;
                case DialogueNodeType::SetVariable:
                    out << "(set: $" << node->variableName << " to " << node->variableValue << ")\n";
                    current = node->nextNodeId;
                    break;
                case DialogueNodeType::Condition: {
                    std::string opStr = "is";
                    switch (node->condition.op) {
                        case DialogueCondition::Op::Equals:      opStr = "is"; break;
                        case DialogueCondition::Op::NotEquals:    opStr = "is not"; break;
                        case DialogueCondition::Op::GreaterThan:  opStr = ">"; break;
                        case DialogueCondition::Op::LessThan:     opStr = "<"; break;
                    }
                    out << "(if: $" << node->condition.variable << " " << opStr << " " << node->condition.value << ")[\n";
                    // We'd need to walk true branch inline — simplified
                    out << "]\n";
                    current = node->nextNodeId;
                    break;
                }
                case DialogueNodeType::Event:
                    out << "<!-- event: " << node->eventName << " -->\n";
                    current = node->nextNodeId;
                    break;
                // Same omission as the Yarn exporter above. Leaving `current`
                // untouched also ended the passage, because the loop's visited
                // check then stopped it, so everything downstream was lost.
                case DialogueNodeType::QuestAction: {
                    const char* verb = "start";
                    switch (node->questAction) {
                        case DialogueNode::QuestActionType::StartQuest:        verb = "start"; break;
                        case DialogueNode::QuestActionType::CompleteObjective: verb = "complete"; break;
                        case DialogueNode::QuestActionType::FailQuest:         verb = "fail"; break;
                    }
                    out << "<!-- quest: " << verb << " " << node->questId;
                    if (node->questAction == DialogueNode::QuestActionType::CompleteObjective)
                        out << " objective " << node->questObjectiveIndex;
                    out << " -->\n";
                    current = node->nextNodeId;
                    break;
                }
                case DialogueNodeType::PlayCinematic:
                    out << "<!-- cinematic: " << node->cinematicEntityName << " -->\n";
                    current = node->nextNodeId;
                    break;
                case DialogueNodeType::SetGameFlag:
                    out << "<!-- flag: " << node->gameFlagKey << " = " << node->gameFlagValue << " -->\n";
                    current = node->nextNodeId;
                    break;
                case DialogueNodeType::End:
                    current = 0;
                    break;
            }
        }
        out << "\n";
        ++passageNum;
    };

    walkPassage(tree.rootNodeId, "Start");

    return out.str();
}

DialogueIOResult TwineIO::Export(const std::string& filepath, const DialogueTreeData& tree) {
    DialogueIOResult result;
    std::string content = ExportToString(tree);

    std::ofstream file(filepath);
    if (!file.is_open()) {
        result.errorMessage = "Failed to write file: " + filepath;
        return result;
    }

    file << content;
    result.success = true;
    result.nodeCount = static_cast<u32>(tree.nodes.size());
    ENJIN_LOG_INFO(Editor, "Exported Twee: %s (%u nodes)", filepath.c_str(), result.nodeCount);
    return result;
}

} // namespace GUI
} // namespace Enjin
