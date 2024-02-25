# ====================================================================================== #
# INFO                                                                                   #
# Author     :    Uran Cabra @ Enchele (uran.cabra@enchele.com)                          # 
# Created on :    01/20/2023                                                             #
# Copyright  :    NectarPD                                                               #
# Description:    PerfectPoint EDM inspection device code. A machine vision application  #
#                 used for meassuring fastener cut offsets. This is the main entrypoint  #
#                 the application.                                                       #
#                                                                                        #
# ====================================================================================== #

import cv2 as cv
import numpy as np
import PySimpleGUI as sg
from filters.CBRGBFilter import CBRGBFilter
from eval import *
from gui import gui, circle


def main():
    """ This is the starting point of the application
        
    """    
    height = 576
    width = 1024
    minradius1 = 97
    maxradius1 = 134
    minradius2 = 220
    maxradius2 = 280
    fastener = "A1"

    fastenerInfo ={
                  "A1":{"actualOffset":0.009, "headDiam":0.296, "GroundTruthFile":"A1.txt"},
                  "A2":{"actualOffset":0.033, "headDiam":0.296, "GroundTruthFile":"A2.txt"},
                  "A3":{"actualOffset":0.020, "headDiam":0.296, "GroundTruthFile":"A3.txt"},
                  "A4":{"actualOffset":0.013, "headDiam":0.296, "GroundTruthFile":"A4.txt"},
                  "A5":{"actualOffset":0.021, "headDiam":0.296, "GroundTruthFile":"A5.txt"},
                  "B1":{"actualOffset":0.013, "headDiam":0.3898, "GroundTruthFile":"B1.txt"},
                  "B2":{"actualOffset":0.009, "headDiam":0.3898, "GroundTruthFile":"B2.txt"},
                  "B3":{"actualOffset":0.011, "headDiam":0.3898, "GroundTruthFile":"B3.txt"},
                  "B4":{"actualOffset":0.007, "headDiam":0.3898, "GroundTruthFile":"B4.txt"},
                  "B5":{"actualOffset":0.011, "headDiam":0.3898, "GroundTruthFile":"B5.txt"},
                  "C1":{"actualOffset":0.008, "headDiam":0.357, "GroundTruthFile":"C1.txt"},
                  "C2":{"actualOffset":0.006, "headDiam":0.357, "GroundTruthFile":"C2.txt"},
                  "C3":{"actualOffset":0.005, "headDiam":0.357, "GroundTruthFile":"C3.txt"},
                  "C4":{"actualOffset":0.012, "headDiam":0.357, "GroundTruthFile":"C4.txt"},
                  "C5":{"actualOffset":0.009, "headDiam":0.357, "GroundTruthFile":"C5.txt"},
                  "D1":{"actualOffset":0.004, "headDiam":0.415, "GroundTruthFile":"D1.txt"},
                  "D2":{"actualOffset":0.010, "headDiam":0.415, "GroundTruthFile":"D2.txt"},
                  "D3":{"actualOffset":0.005, "headDiam":0.415, "GroundTruthFile":"D3.txt"},
                  "D4":{"actualOffset":0.007, "headDiam":0.415, "GroundTruthFile":"D4.txt"},
                  "D5":{"actualOffset":0.005, "headDiam":0.415, "GroundTruthFile":"D5.txt"},
                  "E1":{"actualOffset":0.006, "headDiam":0.3765, "GroundTruthFile":"E1.txt"},
                  "E2":{"actualOffset":0.010, "headDiam":0.3765, "GroundTruthFile":"E2.txt"},
                  "E3":{"actualOffset":0.013, "headDiam":0.3765, "GroundTruthFile":"E3.txt"},
                  "E4":{"actualOffset":0.009, "headDiam":0.3765, "GroundTruthFile":"E4.txt"},
                  "E5":{"actualOffset":0.009, "headDiam":0.3765, "GroundTruthFile":"E5.txt"},
                  "F1":{"actualOffset":0.0732, "headDiam":0.5018, "GroundTruthFile":"F1.txt"},
                  "F2":{"actualOffset":0.00852, "headDiam":0.5018, "GroundTruthFile":"F2.txt"},
                  "F3":{"actualOffset":0.02153, "headDiam":0.5018, "GroundTruthFile":"F3.txt"},
                  "F4":{"actualOffset":0.00950, "headDiam":0.5018, "GroundTruthFile":"F4.txt"},
                  "F5":{"actualOffset":0.01383, "headDiam":0.5018, "GroundTruthFile":"F5.txt"},
                  "G1":{"actualOffset":0.01633, "headDiam":0.302, "GroundTruthFile":"G1.txt"},
                  "G2":{"actualOffset":0.01348, "headDiam":0.302, "GroundTruthFile":"G2.txt"},
                  "G3":{"actualOffset":0.01386, "headDiam":0.302, "GroundTruthFile":"G3.txt"},
                  "G4":{"actualOffset":0.01341, "headDiam":0.302, "GroundTruthFile":"G4.txt"},
                  "G5":{"actualOffset":0.01671, "headDiam":0.302, "GroundTruthFile":"G5.txt"},
                  "H1":{"actualOffset":0.01150, "headDiam":0.339, "GroundTruthFile":"H1.txt"},
                  "H2":{"actualOffset":0.01174, "headDiam":0.339, "GroundTruthFile":"H2.txt"},
                  "H3":{"actualOffset":0.02837, "headDiam":0.339, "GroundTruthFile":"H3.txt"},
                  "H4":{"actualOffset":0.01132, "headDiam":0.339, "GroundTruthFile":"H4.txt"},
                  "H5":{"actualOffset":0.00295, "headDiam":0.339, "GroundTruthFile":"H5.txt"},
                }


    sg.theme('DefaultNoMoreNagging')

    sgLayout = [
                [sg.Text("Detection rate", font="b")],
                [sg.Text("head circle"), sg.Text(size=(10,1), key="-HEADDR-"), sg.Text("inner cut circle"), sg.Text(size=(10,1), key='-INNERDR-')],
                [sg.Text("Offset", font='b')],
                [sg.Text("measured: "), sg.Text(size=(10,1), key="-OFFSET-"), sg.Text("actual: "), sg.Text("0.004\"",size=(10,1), key="-ACTOFFSET-")],
                [sg.Text("Settings", font='b')],
                [sg.Text("Fastner "),sg.InputCombo(list(fastenerInfo.keys()),default_value="A1", key="-FASTENER-", enable_events=True)]
               ]

    gt_inner = []
    gt_outer = []
    inner_percentage = 0
    head_percentage = 0
    offset_samp = 0
    offset_samp_count = 0
    old_offset_samp_count = 0
    mean_offset = 0
    my_gui = gui.gui(buttons=[],touchScreen=True, circles=[circle.circle(x=width//2, y=height//2, radius=minradius1, color=(255,255,0), thickness = 1, label='innermin'),
                                                circle.circle(x=width//2, y=height//2, radius=maxradius1, color=(255,255,0), thickness = 1, label='innermax'),
                                                circle.circle(x=width//2, y=height//2, radius=minradius2, color=(0,200,255), thickness = 1, label='outermin'),
                                                circle.circle(x=width//2, y=height//2, radius=maxradius2, color=(0,100,255), thickness = 1, label='outermax'),
                                                ])
    loadGT(fastenerInfo[fastener]["GroundTruthFile"], gt_inner, gt_outer)
    filter = CBRGBFilter(sgLayout)
    cv.namedWindow("main")
    cam = cv.VideoCapture(1)
    cam.set(cv.CAP_PROP_FRAME_HEIGHT, height)
    cam.set(cv.CAP_PROP_FRAME_WIDTH, width)
    sgWindow = sg.Window('info',sgLayout)
    while cv.getWindowProperty('main', 0) >= 0:
        _,img = cam.read()
        event, value = sgWindow.read(timeout=0)
        filter.updateParam(event, value)
        res, inner, head = filter.run(img,my_gui.getRadiusByLabel('innermin'),
                                           my_gui.getRadiusByLabel('innermax'),
                                           my_gui.getRadiusByLabel('outermin'),
                                           my_gui.getRadiusByLabel('outermax'),
                                           showIntermediarySteps=True
                                           )
        
        if event == "-FASTENER-":
            if value["-FASTENER-"] in fastenerInfo.keys():
                fastener = value["-FASTENER-"]
            sgWindow["-ACTOFFSET-"].update(fastenerInfo[fastener]["actualOffset"])
            offset_samp = 0
            offset_samp_count = 0
            old_offset_samp_count = 0
            mean_offset = 0
            loadGT(fastenerInfo[fastener]["GroundTruthFile"], gt_inner, gt_outer)
            ResetEval()
            filter.resetAcc()

        if len(inner) > 1:
            inner_percentage = eval("inner",np.array(gt_inner), inner)
            sgWindow["-INNERDR-"].update("{:.2f} %".format(inner_percentage*100))
            print("inner: {:.2f}".format(inner_percentage), end="\t")
        else:
        
            # NOTE(uran): 
            # This is just a hacky way of ensuring that we count cases where no circle was found
            # Maybe we should do something else?
            inner_percentage = eval("inner",np.zeros_like([width, height, width]), np.array([width, height, width])) 
        
            sgWindow["-INNERDR-"].update("{:.2f} %".format(inner_percentage*100))
            print("inner: {:.2f}".format(inner_percentage), end="\t")
            
           
        if len(head) > 1:
            head_percentage = eval("head", np.array(gt_outer), head)
            sgWindow["-HEADDR-"].update("{:.2f} %".format(head_percentage*100))
            print("outer: {:.2f}".format(head_percentage), end="\t")
        
        else:
            head_percentage = eval("head",np.zeros_like([width, height, width]), np.array([width, height, width])) 
            sgWindow["-HEADDR-"].update("{:.2f} %".format(head_percentage*100))
            print("head: {:.2f}".format(head_percentage), end="\t")
            
        

        if len(inner) > 1 and len(head) > 1:
            offset_samp += offset(head.astype(np.int16), inner.astype(np.int16), fastenerInfo[fastener]["headDiam"])
            offset_samp_count +=1
            mean_offset = offset_samp/offset_samp_count
            if offset_samp_count - old_offset_samp_count == 10:
                sgWindow["-OFFSET-"].update("{:.6f}\"".format(mean_offset))
                old_offset_samp_count = offset_samp_count
            if offset_samp_count > 100:
                offset_samp_count = 0
                old_offset_samp_count = 0
                offset_samp = 0.0
            
            print("ofsset: {:.4f}".format(mean_offset))
        
        print()


        my_gui.updateGui(res, edit_mode=True, radius_mode=True)
        k = cv.pollKey()

        if k == ord('q'):
            break
        elif k == ord('s'):
            my_gui.showCircles = not my_gui.showCircles 

    cv.destroyAllWindows()
    sgWindow.close()



if __name__ == "__main__":
    main()
