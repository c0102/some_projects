import cv2 as cv
import numpy as np
from gui.gui import *

height = 960
width = 540
fileName = ""

if __name__ == "__main__":
    running = True
    fileName = input("Please enter a file Name: ")
    circlesCount = 0
    currentCircle = "inner"
    mode = ""
    drawing_mode = True
    windowName = 'main'
    edit_mode = radius_mode = center_mode = False
    myGui = gui(windowName=windowName, buttons=[button(x1=410,y1=470,x2=500,y2=500)])
    myGui.circles[0].label = currentCircle
    cam = cv.VideoCapture(1)
    cam.set(cv.CAP_PROP_FRAME_HEIGHT, height)
    cam.set(cv.CAP_PROP_FRAME_WIDTH, width)
    _, img = cam.read()
    running = True
    print('drawing mode | Draw the {} Circle'.format(currentCircle), end='\r')
    while(running):
        _, img = cam.read()
        k = cv.pollKey() & 0xFF
        if k == 27:
            break;
        elif k == ord('d'):
            print("                                                        \rdrawing mode| Draw the {} Circle".format(currentCircle), end='\r')
            drawing_mode = True
            edit_mode = False
        elif k == ord('e'):
            drawing_mode = not drawing_mode
            edit_mode = not edit_mode
        elif k == ord('r'):
            if edit_mode:
                radius_mode = not radius_mode
        elif k == ord('c'):
            if edit_mode:
                center_mode = not center_mode
        elif k == ord('\r'):
            circlesCount += 1 
            if circlesCount == 2:
                myGui.saveCircles(fileName)
                currentCircle = "inner"
                circlesCount = 0
                myGui.removeAllCircles()
                myGui.circles[0].label = currentCircle
                fileName = fileName = input("Please enter a file Name: ")
            else:    
                currentCircle = "outer"
                myGui.addCircle(currentCircle)
        elif k == ord('\b'):
            myGui.removeCircle()
        if radius_mode:
            mode = "radius edit"
        elif center_mode:
            mode = "center edit"
        else:
            mode = ""

        if edit_mode:
            print("                                                       \redit mode {} | Draw the {} Circle".format(mode,currentCircle), end='\r')
        else:
            print("                                                       \rdraw {} | Draw the {} Circle".format(mode,currentCircle), end='\r')
        
        myGui.updateGui(img, drawing_mode, edit_mode, radius_mode, center_mode)
        if(running):
            running = cv.getWindowProperty(windowName,0) >= 0
        
        
    cv.destroyAllWindows()
    

