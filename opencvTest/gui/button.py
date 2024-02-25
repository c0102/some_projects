import cv2 as cv 
import numpy as np

class button(object):
    def __init__(self,img=np.ndarray, x1=-1, y1=-1, x2=-1, y2=-1, color=(200,200,200), thickness = -1, text="Button") -> None:
        self.img = img
        self.x1 = x1
        self.y1 = y1
        self.x2 = x2
        self.y2 = y2
        self.tx = x1 + int((x2-x1)*0.01)
        self.ty = y1 + int((y2-y1)*0.75)
        self.color = color
        self.tcolor = (255-color[0], 255-color[1], 255-color[2])
        self.thickness = thickness
        self.text = text
        self.pressed = False
    

    def setCoordinates(self, coord):
        self.x1 = coord[0]
        self.y1 = coord[1]
        self.x2 = coord[2]
        self.y2 = coord[3]
        self.tx = self.x1 + int((self.x2-self.x1)*0.06)
        self.ty = self.y1 + int((self.y2-self.y1)*0.75)

    def draw(self):
        cv.rectangle(self.img, (self.x1, self.y1), (self.x2, self.y2), self.color, self.thickness )
        cv.putText(self.img, self.text,(self.tx,self.ty), cv.FONT_HERSHEY_SIMPLEX, 0.72, self.tcolor)
    
    def detectPress(self, x, y) -> bool:
        if (x >= self.x1) and (x < self.x2) and (y >= self.y1) and (y <= self.y2):
            print("over button")
        self.pressed =  (x >= self.x1) and (x < self.x2) and (y >= self.y1) and (y <= self.y2)
    
