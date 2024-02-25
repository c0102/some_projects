
def main():
    mlist = []
    tlist = []
    with open('input.txt', 'r') as file:
        for line in file.readlines():
            if len(tlist) == 24:
                mlist.append(tlist.copy())
                tlist = []
                tlist.append(int(line.strip(), base=16))
            else:
                tlist.append(int(line.strip(), base=16))

    with open('out.txt', 'w') as file:
        for line in mlist:
            file.write(str(line).replace("[","{").replace("]","}"))
            file.write(",\n")

    
    return


if __name__ == "__main__":
    main()