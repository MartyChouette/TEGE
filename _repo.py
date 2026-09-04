"""Repo paths for the code generators.

The generators used to hardcode BASE = "D:/GitHub/enjin", so they ran on
exactly one machine at exactly one path. Anyone else who cloned the repo and
edited a shader could not regenerate ShaderData.h, and CI could not either.
Derive it from this file's location instead, the way _gen_api.py already did.
"""
import os

ROOT = os.path.dirname(os.path.abspath(__file__))

SHADERS = os.path.join(ROOT, "Engine", "shaders")


def path(*parts):
    """Absolute path to something in the repo, from repo-relative parts."""
    return os.path.join(ROOT, *parts)
