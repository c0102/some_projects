import numpy as np
gt_inner = []
gt_outer = []
acc = 0
count = 0

radiusDelta = 25
centerDelta = 4
eval_acc = {}

def loadGT(file_name,gt_inner, gt_outer):
    gt_inner.clear() 
    gt_outer.clear()
    try:
        with open(file_name,'r') as file:
            for line in file:
                if len(line) > 1:
                    which,data = line.split(':')
                    if(which == 'inner'):
                        for i in data.split(","):
                            gt_inner.append(int(i))
                    elif(which == 'outer'):
                        for i in data.split(","):
                            gt_outer.append(int(i))
    except FileNotFoundError:
        print("Ground Truth File not Found")

def ResetEval():
    for key in eval_acc.keys():
        eval_acc[key]["acc"] = 0
        eval_acc[key]["count"] = 0


def dist( a, b):
    if (a.shape[0] == 3 and b.shape[0] == 3):
        return np.linalg.norm(a[0:2] - b[0:2])
    return None

def eval(key,a,b):
    if not key in eval_acc.keys():
        eval_acc[key] = {"acc":0, "count":0}
    if eval_acc[key]["count"] > 100000:
        eval_acc[key]["acc"] = 0
        eval_acc[key]["count"] = 0
    d = dist(a,b)
    if( d != None and d <= centerDelta):
        if(abs(a[2] - b[2]) < radiusDelta):
            eval_acc[key]["acc"] += 1
            eval_acc[key]["count"] += 1
        else:
            eval_acc[key]["count"] += 1
    else:
        eval_acc[key]["count"] += 1
    return eval_acc[key]["acc"] / eval_acc[key]["count"]

def offset(headCircle,innerCircle, headDiameterInches):
    d = dist(headCircle,innerCircle)
    offset = None
    if d != None:
        headRadius = headDiameterInches / 2
        scale =  headRadius / headCircle[2]
        offset = scale * d
    return offset

if __name__ == "__main__":
    loadGT("GT.dat", gt_inner, gt_outer)
    inner = []
    outer = []
    while True:
        try:
            inp = input()
        except EOFError:
            break
        try:
            inner, outer = inp.split()
        except :
            print("no")
            continue
        _,in_data = inner.split(":")
        _,out_data = outer.split(":")
        a = []
        b = []
        for i in in_data.split(','):
            a.append(int(i))
        for i in out_data.split(','):
            b.append(int(i))
        print("inner: {}".format(eval("inner",np.array(gt_inner), np.array(a))))
        print("outer: {}".format(eval("head",np.array(gt_outer), np.array(b))))
