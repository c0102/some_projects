# ===================================================================================== #
# INFO                                                                                  #
# Author     :    Uran Cabra @ Enchele (uran.cabra@enchele.com)                         #
# Created on :    01/23/2023                                                            #
# Copyright  :    NectarPD                                                              #
# Description:    This class contains info for drawing and manipulati     #
#                 The library offers very rudimentary gui capabilities hence the        #
#                 need to organize and add some neede functionalities.                  #
#                                                                                       #
# ===================================================================================== #

import numpy as np
import cv2 as cv


class circle(object):
    def __init__(self,img=np.zeros(1), x=-1, y=-1, radius=-1, color=(0,0,0), thickness=1, label="", lockSelection=False):
        self.img = img
        self.x = x
        self.y = y
        self.center = (x,y)
        self.radius = radius
        self.thickness = thickness
        self.detThickness = thickness+1
        self.color = color
        self.PerimeterDelta = 3
        self.selected = False
        self.label = label
        self.lockSelection = lockSelection
    
    def drawCircle(self):
        if self.selected:
            cv.circle(self.img, (self.x,self.y),self.radius,self.color,self.detThickness)
        else:
            cv.circle(self.img, (self.x,self.y),self.radius,self.color,self.thickness)
        
        # cv.circle(self.img, (self.x,self.y), self.radius + self.PerimeterDelta, (255,255,255), self.thickness)
        # cv.circle(self.img, (self.x,self.y), np.max((self.radius - self.PerimeterDelta, 0)), (255,255,255), self.thickness)
    
    def setRadius(self, r:int):
        self.radius = r
    
    def setCenter(self, x,y):
        self.x = x
        self.y = y

    def detectSelection(self, x, y):
        dist = np.linalg.norm((self.x-x, self.y-y))
        self.selected = (dist <= (self.PerimeterDelta + self.radius)) and (dist >= (self.radius - self.PerimeterDelta))
            
    def setRadius(self,x,y):
        self.radius = int(np.linalg.norm((self.x - x, self.y - y)))

    def setCenter(self, x,y):
        self.x = x
        self.y = y
    
    def saveToFile(self, file_name):
        with open(file_name, 'a') as file:
            file.write("{}:{},{},{}\n".format(self.label,self.x, self.y,self.radius))
            