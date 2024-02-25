import cv2 as cv
import numpy as np
from filters.helpers import *

class filter():
    def __init__(self) -> None:
        pass

class contrastFilter(filter):
    def __init__(self, params) -> None:
        super().__init__()
        self.params = params
    
    def run(self, img):
        return contrastIncr(img, *self.params)
    def set_params(self, params):
        self.params = params
    def get_params(self,params):
        return self.params
    def __str__(self) -> str:
        return "clipLimit:{}, TileGridX:{}, TileGridY:{}".format(*self.params)

class toGrayFilter(filter):
    def  __init__(self) -> None:
        super().__init__()
        self.params = []
    def run(self, img):
        return grayfilter(img)
    def set_params(self, params):
        self.params = params
    def get_params(self,params):
        return self.params
    def __str__(self) -> str:
        return "Convet img to GrayScale has no parmeters"

class   GrayBrightenFilter(filter):
    def  __init__(self, params) -> None:
        super().__init__()
        self.params = params
    def run(self, img):
        return GrayIncrFilter(img, *self.params)
    def set_params(self, params):
        self.params = params
    def get_params(self,params):
        return self.params
    def __str__(self) -> str:
        return "Gray Image Brighten percentage: {}".format(*self.params)

class   DarkLightenFilter(filter):
    def  __init__(self) -> None:
        super().__init__()
        self.params = []
    def run(self, img):
        return Lighten(img)
    def set_params(self, params):
        self.params = params
    def get_params(self,params):
        return self.params
    def __str__(self) -> str:
        return "Brighten dark areas has no further params"

class   MedianBlurFilter(filter):
    def  __init__(self, params) -> None:
        super().__init__()
        self.params = params
    def run(self, img):
        return cv.medianBlur(img, *self.params)
    def set_params(self, params):
        self.params = params
    def get_params(self):
        return self.params
    def __str__(self) -> str:
        return "kernel size: {}".format(*self.params)

class   GaussianBlurFilter(filter):
     def  __init__(self, params) -> None:
        super().__init__()
        self.params = params
     def run(self, img):
        return cv.GaussianBlur(img, *self.params)
     def set_params(self, params):
        self.params = params
     def get_params(self):
        return self.params
     def __str__(self) -> str:
        return "kernel size: {}, SigmaX: {}".format(*self.params)

class   adaptiveThresholdFilter(filter):
    def  __init__(self, params) -> None:
        super().__init__()
        self.params = params
    def run(self, img):
        return cv.adaptiveThreshold(img,*self.params)
    def set_params(self, params):
        self.params = params
    def get_params(self):
        return self.params
    def __str__(self) -> str:
        return "Max Value: {}, Adaptive Method: {}, Threshold Type: {}, Block Size: {}, Constant: {}".format(*self.params)