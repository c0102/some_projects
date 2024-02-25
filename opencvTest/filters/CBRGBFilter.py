import cv2 as cv
import numpy as np
import PySimpleGUI as sg
from filters.helpers import *
from filters.filter import *


class CBRGBFilter(object):
    def __init__(self, layout, param1 = 100, param2 = 30, ksize = 23, clipLimit = 22.0, tilegridy = 2, 
                tilegridx = 5, d = 5, sigmaColor = 150, sigmaSpace = 150, maxvalue = 255, blockSize = 3,
                adaptive_method="ADAPTIVE_THRESH_GAUSSIAN", threshold_type="THRESH_BINARY", C=2, grey_brighten = 5, red_increase = 0.1, blue_increase = 0.1) -> None:
        
        self.AccInner = np.empty((0,3))
        self.AccOuter = np.empty((0,3))
        self.param1 = param1
        self.param2 = param2
        self.ksize     = ksize    
        self.clipLimit = clipLimit/10
        self.tilegridx = tilegridx
        self.tilegridy = tilegridy
        self.d = d 
        self.red_increase = red_increase 
        self.blue_increase = blue_increase 
        self.gray_brighten = grey_brighten /100.0
        self.sigmaSpace = sigmaSpace
        self.sigmaColor = sigmaColor
        self.maxValue = maxvalue
        self.adaptive_methods = {"ADAPTIVE_THRESH_MEAN":cv.ADAPTIVE_THRESH_MEAN_C,"ADAPTIVE_THRESH_GAUSSIAN":cv.ADAPTIVE_THRESH_GAUSSIAN_C}
        self.threshold_types = {"THRESH_BINARY":cv.THRESH_BINARY, "THRESH_BINARY_INV":cv.THRESH_BINARY_INV}
        self.adaptive_method = adaptive_method
        self.threshold_type = threshold_type
        self.blockSize = blockSize
        self.C = C
        self.sgLayout = [
            
            [sg.Text('ksize: ' ), sg.Slider(range=(1,200), default_value=ksize, orientation='h', key="-KSIZESLIDER-", resolution=2, enable_events=True)],
            [sg.Text('clipLimit: '), sg.Slider(range=(1,300), default_value=clipLimit, orientation='h', key="-CLIPLIMITSLIDER-", enable_events=True)],
            [sg.Text('tilegridx: '), sg.Slider(range=(1,100), default_value=tilegridx, orientation='h', key="-TILEGRIDXSLIDER-",enable_events=True)],
            [sg.Text('tilegridy: '), sg.Slider(range=(1,100), default_value=tilegridy, orientation='h', key="-TILEGRIDYSLIDER-",enable_events=True)],
            [sg.Text('Blue increase percentage: '), sg.Slider(range=(0.01,1.0), default_value=blue_increase, resolution=0.01, orientation='h', key="-BLUEINCREASESLIDER-",enable_events=True)],
            [sg.Text('Red increase percentage: '), sg.Slider(range=(0.01,1.0), default_value=red_increase,resolution=0.01, orientation='h', key="-REDINCREASESLIDER-",enable_events=True)],
            [sg.Text('grey brighten percentage: '), sg.Slider(range=(1,100), default_value=grey_brighten, orientation='h', key="-GREYBRIGHTENSLIDER-",enable_events=True)],
            
            # [sg.Text('d: '), sg.Slider(range=(1,300), default_value=d, orientation='h', key="-DSLIDER-",enable_events=True)],
            # [sg.Text('sigmaColor: '), sg.Slider(range=(1,300), default_value=sigmaColor, orientation='h', key="-SIGMACOLORSLIDER-",enable_events=True)],
            # [sg.Text('sigmaSpace: '), sg.Slider(range=(1,300), default_value=sigmaSpace, orientation='h', key="-SIGMASPACESLIDER-",enable_events=True)],
            [sg.Text('Thresholding', font='b')],
            [sg.Text("Adaptive Method"), sg.InputCombo(["ADAPTIVE_THRESH_MEAN", "ADAPTIVE_THRESH_GAUSSIAN"], default_value=adaptive_method, key="-ADAPTIVEINPUT-", enable_events=True),
             sg.Text("Threshold Type"), sg.InputCombo(["THRESH_BINARY", "THRESH_BINARY_INV"], default_value=threshold_type, key="-THRESHOLDTYPEINPUT-", enable_events=True)],
            [sg.Text("Max Value"), sg.Slider(range=(0,255), default_value=maxvalue, orientation='h', key="-MAXVALUESLIDER-", enable_events=True)],
            [sg.Text("Block Size"), sg.Slider(range=(3,101), default_value=blockSize, orientation='h', key="-BLOCKSIZESLIDER-",resolution=2, enable_events=True)],
            [sg.Text("Constant"), sg.Slider(range=(0,100), default_value=C, orientation='h', key="-CSLIDER-", enable_events=True)],


            ]

        # self.filters = [contrastFilter([self.clipLimit, self.tilegridx, self.tilegridy]), 
        #                 toGrayFilter(), GrayBrightenFilter([self.gray_brighten]), 
        #                 DarkLightenFilter(), 
        #                 MedianBlurFilter([self.ksize]),
        #                 adaptiveThresholdFilter([self.maxValue, self.adaptive_methods[self.adaptive_method], self.threshold_types[self.threshold_type], self.blockSize, self.C])]

        for i in self.sgLayout:
            layout.append(i)
    
    def run(self, img, minradius1, maxradius1, minradius2, maxradius2, showIntermediarySteps=False):
        if showIntermediarySteps:
            scale_percent = 40 # percent of original size
            width = int(img.shape[1] * scale_percent / 100)
            height = int(img.shape[0] * scale_percent / 100)
            dim = (width, height)

        # final = self.filters[0].run(img)
        # for filt in self.filters[0:]:
        #     final = filt.run(final)

        filt_img = contrastIncr(img, self.clipLimit, self.tilegridx, self.tilegridy)
        if showIntermediarySteps:
            resized = cv.resize(filt_img, dim, interpolation = cv.INTER_AREA)
            cv.imshow("Contrast step", resized)
        
        filt_img = BIncrFilter(filt_img, self.blue_increase)
        if showIntermediarySteps:
            resized = cv.resize(filt_img, dim, interpolation = cv.INTER_AREA)
            cv.imshow("Blue Increase step", resized)
        
        filt_img = RIncrFilter(filt_img, self.red_increase)
        if showIntermediarySteps:
            resized = cv.resize(filt_img, dim, interpolation = cv.INTER_AREA)
            cv.imshow("Red Increase step", resized)
        
        gray_img = grayfilter(filt_img)
        if showIntermediarySteps:
            resized = cv.resize(gray_img, dim, interpolation = cv.INTER_AREA)
            cv.imshow("To Gray", resized)
        
        gray_img = GrayIncrFilter(gray_img, self.gray_brighten)
        if showIntermediarySteps:
            resized = cv.resize(gray_img, dim, interpolation = cv.INTER_AREA)
            cv.imshow("Gray Brighten", resized)
        
        gray_img = Lighten(gray_img)
        if showIntermediarySteps:
            resized = cv.resize(gray_img, dim, interpolation = cv.INTER_AREA)
            cv.imshow("Lighten Dark", resized)
        
        final =  cv.medianBlur(gray_img, self.ksize)
        if showIntermediarySteps:
            resized = cv.resize(final, dim, interpolation = cv.INTER_AREA)
            cv.imshow("Blur", resized)
        
        final =  cv.adaptiveThreshold(final,self.maxValue,self.adaptive_methods[self.adaptive_method], self.threshold_types[self.threshold_type],self.blockSize,self.C)
        if showIntermediarySteps:
            resized = cv.resize(final, dim, interpolation = cv.INTER_AREA)
            cv.imshow("Threshold", resized)
        

        # final =  cv.bilateralFilter(final, self.d, self.sigmaColor,self.sigmaSpace)
        # if showIntermediarySteps:
        #     resized = cv.resize(final, dim, interpolation = cv.INTER_AREA)
        #     cv.imshow("Bilateral", resized)
        
        inner = []
        outer = []
        rows = final.shape[0]
        in_circles = cv.HoughCircles(final, cv.HOUGH_GRADIENT, 1, rows / 8,
                               param1=self.param1, param2=self.param2,
                               minRadius=minradius1, maxRadius=maxradius1)
        
        if in_circles is not None:
            # in_circles = np.uint16(np.around(in_circles))
            inner = in_circles[0][0]
            self.AccInner = np.append(self.AccInner, [inner], axis=0)
            # if(self.AccInner.size() != 
            inner = np.average(self.AccInner, axis=0)
            inner = np.uint16(np.around(inner))
            center = (inner[0], inner[1])
            cv.circle(img, center, 1, (255, 255 , 255), 3)
            radius = inner[2]
            cv.circle(img, center, radius, (255, 0, 255), 3)
            if(self.AccInner.shape[0] > 100):
                self.resetAccInner()
        out_circles = cv.HoughCircles(final, cv.HOUGH_GRADIENT, 1, rows / 8,
                               param1=self.param1, param2=self.param2,
                               minRadius=minradius2, maxRadius=maxradius2)
        
        if out_circles is not None:
            # out_circles = np.uint16(np.around(out_circles))
            outer = out_circles[0][0]
            self.AccOuter = np.append(self.AccOuter, [outer], axis=0)
            outer = np.average(self.AccOuter, axis=0)
            outer = np.uint16(np.around(outer))
            center = (outer[0], outer[1])
            cv.circle(img, center, 1, (255, 255 , 255), 3)
            radius = outer[2]
            cv.circle(img, center, radius, (255, 0, 255), 3)
            if(self.AccOuter.shape[0] > 100):
                self.resetAccOuter()
        return (img, inner, outer)

    def resetAcc(self):
        self.resetAccInner()
        self.resetAccOuter()
    def resetAccInner(self):
        self.AccInner = np.empty((0,3))
    def resetAccOuter(self):
        self.AccOuter = np.empty((0,3))

    def updateParam(self, event, value):
        if event == "-PARAM1SLIDER-":
            self.param1 = int(value["-PARAM1SLIDER-"])
        if event == "-PARAM2SLIDER-":
            self.param2 = int(value["-PARAM2SLIDER-"])
        if event == "-KSIZESLIDER-":
            self.ksize = int(value["-KSIZESLIDER-"])
        if event == "-CLIPLIMITSLIDER-":
            self.clipLimit = value["-CLIPLIMITSLIDER-"]/10.0
        if event == "-TILEGRIDXSLIDER-":
            self.tilegridx = int(value["-TILEGRIDXSLIDER-"])
        if event == "-TILEGRIDYSLIDER-":
            self.tilegridy = int(value["-TILEGRIDYSLIDER-"])
        if event == "-DSLIDER-":
            self.d = int(value["-DSLIDER-"])
        if event == "-SIGMACOLORSLIDER-":
            self.sigmaColor = int(value["-SIGMACOLORSLIDER-"])
        if event == "-SIGMASPACESLIDER-":
            self.sigmaSpace = int(value["-SIGMASPACESLIDER-"])
        if event == "-ADAPTIVEINPUT-":
            if value["-ADAPTIVEINPUT-"] in self.adaptive_methods.keys():
                self.adaptive_method = value["-ADAPTIVEINPUT-"]
        if event == "-THRESHOLDTYPEINPUT-":
            if value["-THRESHOLDTYPEINPUT-"] in self.threshold_types.keys():
                self.threshold_type = value["-THRESHOLDTYPEINPUT-"]
        if event == "-MAXVALUESLIDER-":
            self.maxValue = int(value["-MAXVALUESLIDER-"])
        if event == "-BLOCKSIZESLIDER-":
            self.blockSize = int(value["-BLOCKSIZESLIDER-"])
        if event == "-CSLIDER-":
            self.C = int(value["-CSLIDER-"])
        if event == "-GREYBRIGHTENSLIDER-":
            self.gray_brighten = int(value["-GREYBRIGHTENSLIDER-"]) / 100.0
        if event == "-BLUEINCREASESLIDER-":
            self.blue_increase = value["-BLUEINCREASESLIDER-"]        
        if event == "-REDINCREASESLIDER-":
            self.blue_increase = value["-REDINCREASESLIDER-"]        




