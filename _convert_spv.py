import sys, struct

def spv_to_uchar(p):
    with open(p,"rb") as f:
        data=f.read()
    s=len(data); w=s//4; lines=[]
    for i in range(0,s,16):
        chunk=data[i:i+16]
        hv=", ".join("0x{:02x}".format(b) for b in chunk)
        lines.append("    "+hv)
    return ",\n".join(lines),s,w

def spv_to_u32(p):
    with open(p,"rb") as f:
        data=f.read()
    s=len(data); w=s//4; vals=[]
    for i in range(0,s-3,4):
        v=struct.unpack("<I",data[i:i+4])[0]
        vals.append("0x{:08X}".format(v))
    lines=[]
    for i in range(0,len(vals),5):
        chunk=vals[i:i+5]
        lines.append("    "+", ".join(chunk))
    return ",\n".join(lines),s,w

if __name__=="__main__":
    fmt=sys.argv[1]; spv=sys.argv[2]
    out=sys.argv[3] if len(sys.argv)>3 else None
    fn=spv_to_uchar if fmt=="uchar" else spv_to_u32
    content,size,words=fn(spv)
    if out:
        with open(out,"w",newline="\n") as f: f.write(content)
        print("SIZE:{} WORDS:{} FILE:{}".format(size,words,out))
    else:
        print(content)
        print("SIZE:{} WORDS:{}".format(size,words),file=sys.stderr)
