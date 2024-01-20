from fastapi import FastAPI, HTTPException, status, Response
import pathlib
import os

root_dir = "root"
next_inode = 1001

inode2names = {
    1000: root_dir
}

inode2type = {
    1000: 'd'
}

for (dirpath, dirnames, filenames) in os.walk(root_dir):
    for dir in dirnames:
        dir_full_path = os.path.join(dirpath, dir)
        inode = next_inode
        next_inode += 1
        inode2names[inode] = dir_full_path
        inode2type[inode] = 'd'
    for file in filenames:
        file_full_path = os.path.join(dirpath, file)
        inode = next_inode
        next_inode += 1
        inode2names[inode] = file_full_path
        inode2type[inode] = 'f'

print(inode2names)
print(inode2type)

app = FastAPI()

def inode_from_name(name: str) -> int:
    filtered_inodes = list(filter(lambda x: x[1] == name, inode2names.items()))
    inode = filtered_inodes[0][0]
    return inode

@app.get("/lookup")
def lookup(inode: int, name: str):
    if inode not in inode2names:
        raise HTTPException(status.HTTP_404_NOT_FOUND)
    dir = inode2names[inode]
    path = pathlib.Path(f'{dir}/{name}')
    
    if not path.exists():
        raise HTTPException(status.HTTP_404_NOT_FOUND)

    res = 'f' if path.is_file() else 'd'
    res += str(inode_from_name(str(path)))

    return Response(res)

@app.get("/list")
def list_dir(inode: int):
    if inode not in inode2type:
        raise HTTPException(status.HTTP_404_NOT_FOUND)

    if inode2type[inode] != 'd':
        raise HTTPException(status.HTTP_400_BAD_REQUEST)

    dir = inode2names[inode]
    
    entries = os.listdir(dir)

    resp = str(len(entries)) + '\n'

    for entry in entries:
        full_path = os.path.join(dir, entry)
        entry_inode = inode_from_name(full_path)
        entry_type = 'f' if pathlib.Path(full_path).is_file() else 'd'
        resp += f'{entry_type} {entry_inode} {entry}\n'

    return Response(resp)

@app.get("/create")
def create(parent_inode: int, name: str, create_type: str):
    global next_inode

    if parent_inode not in inode2names:
        return HTTPException(status.HTTP_404_NOT_FOUND)

    if create_type != 'f' and create_type != 'd':
        return HTTPException(status.HTTP_400_BAD_REQUEST)

    dir = inode2names[parent_inode]
    full_path = os.path.join(dir, name)
    
    if pathlib.Path(full_path).exists():
        return HTTPException(status.HTTP_409_CONFLICT)

    new_inode = next_inode
    next_inode += 1

    inode2names[new_inode] = full_path
    inode2type[new_inode] = create_type

    if create_type == 'f':
        file = open(full_path, 'w')
        file.close()
    else:
        os.mkdir(full_path)

    return Response(str(new_inode))

@app.get("/unlink")
def unlink(parent_inode: int, name: str, delete_type: str):
    if parent_inode not in inode2names:
        return HTTPException(status.HTTP_404_NOT_FOUND)

    dir = inode2names[parent_inode]
    full_path = os.path.join(dir, name)
    to_delete_inode = inode_from_name(full_path)
    node_type = inode2type[to_delete_inode]

    if node_type != delete_type:
        return HTTPException(status.HTTP_400_BAD_REQUEST)

    if delete_type == 'f':
        os.remove(full_path)
    else:
        os.rmdir(full_path)

    del inode2names[to_delete_inode]
    del inode2type[to_delete_inode]

    return ""

@app.get("/read_size")
def read_size(inode: int):
    if inode not in inode2names:
        raise HTTPException(status.HTTP_404_NOT_FOUND)

    if inode2type[inode] != 'f':
        raise HTTPException(status.HTTP_400_BAD_REQUEST)

    with open(inode2names[inode], 'rb') as file:
        data = file.read()
        return Response(str(len(data)))

@app.get("/read")
def read(inode: int):
    if inode not in inode2names:
        raise HTTPException(status.HTTP_404_NOT_FOUND)

    if inode2type[inode] != 'f':
        raise HTTPException(status.HTTP_400_BAD_REQUEST)

    with open(inode2names[inode], 'rb') as file:
        data = file.read()
        return Response(data)

@app.get("/write")
def write(inode: int, content: str):
    if inode not in inode2names:
        raise HTTPException(status.HTTP_404_NOT_FOUND)

    if inode2type[inode] != 'f':
        raise HTTPException(status.HTTP_400_BAD_REQUEST)

    with open(inode2names[inode], 'wb') as file:
        b = [ord(char) for char in content]
        file.write(bytes(b))

    return Response("")