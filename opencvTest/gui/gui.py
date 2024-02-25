# ===================================================================================== #
# INFO                                                                                  #
# Author     :    Uran Cabra @ Enchele (uran.cabra@enchele.com)                         #
# Created on :    01/23/2023                                                            #
# Copyright  :    NectarPD                                                              #
# Description:    This module provides gui utilities related to the OpenCV library.     #
#                 The library offers very rudimentary gui capabilities hence the        #
#                 need to organize and add some neede functionalities.                  #
#                                                                                       #
# ===================================================================================== #
import numpy as np
import cv2 as cv
from random import randint
from .circle import circle
from .button import button 


class gui(object):
    def __init__(self, circles=[circle(x=0, y=0, radius=1, color=(255,0,0))], buttons=[button(x1=1,y1=1,x2=1,y2=1)], in_img = np.zeros((512,512,3), np.uint8), windowName='main', showCircles=True, touchScreen = False):
        """ Constructor  for the gui object.

        Args:
            circles (list, optional): A list of Circles to be displayed on the main window. Defaults to [circle(x=0, y=0, radius=1, color=(255,0,0))] which is a non visible circle.
            buttons (list, optional): A list of Buttons to be displayed on the main window. Defaults to [button(x1=410,y1=470,x2=500,y2=500)].
            in_img (np.array like, optional): The image that will be displayed on the main window. Defaults to np.zeros((512,512,3), np.uint8).
            windowName (str, optional): The name of the window. Defaults to 'main'.
            showCircles (bool, optional): This should be True if you want to show the circles by default. Defaults to True.
            touchScreen (bool, optional): This should be True if the interface is a touch screen. Circle manipulation is 
                                          different for touch screen interfaces. Defaults to False.
        """        
        self.drawing_mode = False
        self.edit_mode = False
        self.radius_mode = False
        self.center_mode = False
        self.touchScreen = touchScreen
        self.current = 0
        self.img = in_img
        self.circles = circles
        self.drawing = False
        self.ix,iy = -1,-1
        self.ex,ey = -1,-1
        self.showCircles = showCircles
        self.buttons = buttons
        self.windowName = windowName
        cv.namedWindow(self.windowName, flags=1)
        # cv.createButton("Back",back,None,cv.QT_PUSH_BUTTON,1)
        cv.setMouseCallback(self.windowName,Mouse_callback, param=self)

    def updateGui(self,in_img=None, drawing_mode=False, edit_mode=False, radius_mode=False,  center_mode=False):
        """ This method is used to update the image on the window controled by the gui instance

        Args:
            in_img (numpy array like, optional): The image to be displayed. If not given, 
                                                 the image at object instantiation is used instead. Defaults to None.
            drawing_mode (bool, optional): If True enter Circle drawing mode. Defaults to False.
            edit_mode (bool, optional): If True enter Circle editing mode. Defaults to False.
            radius_mode (bool, optional): If True while in Circle editing mode, edit the radius of the circle. 
                                          Has precedence over center editing. Defaults to False.
            center_mode (bool, optional): If True while in Circle editing mode, edit the center of the circle. 
                                          Not functional if True at the same time as radius_mode. Defaults to False.

        """        
        if in_img is not None:
            self.img = in_img
        self.drawing_mode=drawing_mode
        self.edit_mode = edit_mode
        self.radius_mode = radius_mode
        self.center_mode = center_mode
        if self.showCircles:
            for circle in self.circles:
                circle.img = in_img
                circle.drawCircle()
        for button in self.buttons:
            button.img = in_img
            button.draw()
        
        cv.imshow(self.windowName, self.img)
      
    
    
    def saveCircles(self,file_name='GT.txt'):
        """ Method to save Circle info to a file. This calls saveToFile method for each 
            Circle in the gui instance meaning, the actual writing is handled by each 
            circle 

        Args:
            file_name (str, optional): The filename to be used. Defaults to 'GT.txt'.
        """         
        for circle in self.circles:
            circle.saveToFile(file_name)

    def addCircle(self,label="",x=0,y=0, radius=1, color=(randint(0,255),randint(0,255),randint(0,255)), thickness=1):
        """ Method to add circles to the gui instance. 

        Args:
            label (str, optional):   Name the circle. This label can be used to find specific circles as well as
                                        when we save the circle to a file. Defaults to "".
            
            x (int, optional):       Circle's center x coordinate. Defaults to 0.
            
            y (int, optional):       Circle's center y coordinate. Defaults to 0.
            
            radius (int, optional):  Circle's radius. Defaults to 1.
            
            color (tuple, optional): Color of the perimeter of the circle. 
                                     A tuple of three ints in (R,G,B). Defaults to (randint(0,255),randint(0,255),randint(0,255)).
            
            thickness (int, optional): Thickness of the circle's perimeter. If thickness is -1 the circle area is 
                                       filled with color as well. Defaults to 1.
        """               
        self.circles.append(circle(img=self.img, x=x,y=y,radius=radius, color=color, label=label, thickness=thickness))
        self.current = len(self.circles) - 1
    
    def removeCircle(self, label = None):
        """ This method is used to remove a circle from the gui instance. 
            It can remove a selected circle or by label

        Args:
            label (str, optional): If the label is given the circle with that label is removed. 
                                   Otherwise a selected circle is removed. Defaults to None.
        """        
        for index, circle in enumerate(self.circles):
            if label  != None:
                if circle.label == label:
                    self.circles.pop(index)
                if self.current > 0:
                    self.current -= 1
                if self.current == 0:
                    self.current = -1
                    self.addCircle()
            elif circle.selected:
                self.circles.pop(index)
                if self.current > 0:
                    self.current -= 1
                if self.current == 0:
                    self.current = -1
                    self.addCircle()
    
    def removeAllCircles(self):
        """
            This method removes all previous circles from the gui instance. Another circle with
            center 0,0 and radius 1 is added instead since we need at least a circle for rendering
            
            TODO(uran): Change the handling of gui update to not require a circle to be present
        """        
        self.circles.clear()
        self.addCircle()


    def getRadiusByLabel(self, label):
        """ Method to get the radius of a circle by its label. 

        Args:
            label (str): the label of the circle who's radius is required 

        Returns:
            int: circle's radius if found. None if circle not found 
        """        
        for circle in self.circles:
            if circle.label == label:
                return circle.radius
        return None



def button_cb(param):
    for circle in param.circles:
        if circle.selected:
            circle.color = (randint(0,255),randint(0,255),randint(0,255))

def Mouse_callback(event,x,y,flags,param):
        """ 
            This function is used as a callback for handling mouse events on the OpenCV main window
            This is used for drawing and manipulating circles.
        """        
        if event == cv.EVENT_LBUTTONDOWN:
            for button in param.buttons:
                button.detectPress(x,y)
                if button.pressed:
                    # print("over button: " + str(button_pressed))
                    button_cb(param)
                    return
            if param.drawing_mode:
                    param.drawing = True
                    param.circles[param.current].selected = False
                    param.circles[param.current].thickness = 1  
                    param.circles[param.current].setCenter(x,y)
                    param.circles[param.current].setRadius(x,y)
            elif param.edit_mode:
                for circle in param.circles:
                    circle.detectSelection(x,y)
        elif event == cv.EVENT_MOUSEMOVE:
            # print("{}, {}".format(x,y))
            # param.circles[param.current].detectSelection(x,y)
            if param.edit_mode:
                for circle in param.circles:
                    # circle.detectSelection(x,y)
                    if circle.selected:
                        if param.radius_mode:
                            circle.setRadius(x,y)
                        elif param.center_mode:
                            circle.setCenter(x,y)
    
            if param.drawing == True:
                param.circles[param.current].setRadius(x,y)
    
        elif event == cv.EVENT_LBUTTONUP:
            if param.touchScreen:
                for circle in param.circles:
                    circle.selected = False
            param.drawing = False
        elif event == cv.EVENT_RBUTTONDOWN:
            for circle in param.circles:
                circle.selected = False


# TODO(uran): begin adding tests
if __name__ == "__main__":
    myGui = gui()
    
    while True:
        cv.pollKey()
        myGui.updateGui()
    
