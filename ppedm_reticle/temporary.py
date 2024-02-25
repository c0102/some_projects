#!/home/orangepi/ppedm_reticle/venv/bin/python
#v0.0.2 - 2/2/24 | added logging 

import sys
import cv2 as cv
import numpy as np
from math import sqrt, atan2, cos, sin
import json
import sys
import os
import subprocess 
from PySide6.QtCore import Qt, Slot, Signal, QThread, QTimer
from PySide6.QtGui import QImage, QPixmap, QPainter, QColor, QFont
from PySide6.QtWidgets import QApplication, QLabel, QMainWindow, QVBoxLayout, QWidget
import logging 
logging.basicConfig(filename=os.path.join(os.path.dirname(os.path.realpath(__file__)),"errors.log"), encoding='utf-8', level=logging.DEBUG)


config = {
            "IMAGE_RESIZE_RATIO" :                        0.8, # BY HOW MUCH DO WE RESIZE THE IMAGE BEFORE PROCESSING
            "ACCEPTABLE_DISTANCE_FROM_CENTER" :           5.0,                                           
            "INDICATOR_CIRCLE_RADIUS_WIDTH_RATIO" :       0.35, # A NUMBER FROM 0.1 TO 1.0, WITH 1.0 BEING THE FULL WIDTH AS THE CIRCLE RADIUS
            "INDICATOR_CIRCLE_THICKNESS" :                20,
            "INDICATOR_CIRCLE_NOT_CENTER_COLOUR" :        (50,50,255), # COLOR IS SPECIFIED IN BGR - Blue Green Red format, EACH NUMBER GOES FROM 0 - 255
            "INDICATOR_CIRCLE_CENTER_COLOUR" :            (100,255,100), # COLOR IS SPECIFIED IN BGR - Blue Green Red format, EACH NUMBER GOES FROM 0 - 255
            "DRAW_FASTENER_CIRCLE" :                      False, # False TO STOP DRAWING THE GRAY CIRCLE
            "DRAW_CROSSHAIRS" :                           True, # False TO STOP DRAWING THE CROSSHAIRS
            "CROSSHAIRS_COLOR" :                          (0,0,0), # COLOR IS SPECIFIED IN BGR - Blue Green Red format, EACH NUMBER GOES FROM 0 - 255
            "MEDIAN_BLUR" :                               11, # SHOULD BE AN ODD NUMBER STARTING FROM 3. 
            "DRAW_DIRECTION_INDICATOR" :                  True,
            "DRAW_SCAN_ZONE_CIRCLES"  :                   False, 
            "MIN_CIRCLE_RADIUS" :                         50, 
            "MAX_CIRCLE_RADIUS" :                         150,
            "FULLSCREEN_IMAGE_RESIZE_MULTIPLIER" :        1.25,
            "FULLSCREEN_IMAGE_X_OFFSET" :                 0,
            "FULLSCREEN_IMAGE_Y_OFFSET" :                 0,
            "CROSSHAIRS_X_OFFSET" :                       0,
            "CROSSHAIRS_Y_OFFSET" :                       0,
            "INDICATOR_CIRCLE_X_OFFSET" :                 0,
            "INDICATOR_CIRCLE_Y_OFFSET" :                 0,
        }


def save_config():
    global config
    script_directory = os.path.dirname(os.path.realpath(__file__))
    sfile = os.path.join(script_directory, "config.json")
    logging.debug("[SAVE]: config file will be saved as {}".format(sfile))
    try:
        with open(sfile, 'w') as json_file:
            json.dump(config, json_file, indent='\t')#separators=(',\n',': '))
            json_file.flush()
            os.fsync(json_file.fileno())
            logging.debug("[SAVE]: saved config file at {}".format(sfile))
    except Exception as e:
        print(e)
        logging.error(e)
        
def load_config():
    global config
    script_directory = os.path.dirname(os.path.realpath(__file__))
    sfile = os.path.join(script_directory, "config.json")
    logging.debug("[LOAD]: config file located at {}".format(sfile))
    try:
        with open(sfile, 'r') as json_file:
            config = json.load(json_file)
            logging.debug("[LOAD]: loaded config file from {}".format(sfile))
    except Exception as e:
        print(e)
        logging.error(e)


# import debugpy
class MainImageProcThread(QThread):
    updateFrame = Signal(QImage)
    def __init__(self, parent=None):
        QThread.__init__(self,parent)
        self.Accumulator = np.empty((1,3))
        self.circle = []
        if sys.platform.startswith("linux"):
            self.cam = cv.VideoCapture(1)   #'videos\moving_zoomed_in.mp4')#1)
        else:
            self.cam = cv.VideoCapture(0)   #'videos\moving_zoomed_in.mp4')#1)

        self.status = True
        self.nwidth  = 0
        self.nheight = 0

    
    def stop(self):
        self.cam.release()
        self.status = False

    def run(self):
        # debugpy.debug_this_thread()
        # cam.set(cv.CAP_PROP_FRAME_HEIGHT, height)
        # cam.set(cv.CAP_PROP_FRAME_WIDTH, width)
        while self.status:
            ret,img = self.cam.read()
            if not ret:
                # print("No image returned from camera!")
                continue
            if ret:

                height, width, _ = img.shape
                self.nheight = int(height* config["IMAGE_RESIZE_RATIO"])
                self.nwidth = int(width* config["IMAGE_RESIZE_RATIO"])
                img = cv.resize(img, (self.nwidth, self.nheight), cv.INTER_LINEAR)
                r = int(self.nwidth*config["INDICATOR_CIRCLE_RADIUS_WIDTH_RATIO"]) #To be used for the croshair circle radius, arbitrarily 20% of the width of the frame
                self.mid_x = (self.nwidth  // 2) - config["FULLSCREEN_IMAGE_X_OFFSET"] 
                self.mid_y = (self.nheight // 2 ) - config["FULLSCREEN_IMAGE_Y_OFFSET"]
                self.x_crosshair = self.mid_x + config["CROSSHAIRS_X_OFFSET"]
                self.y_crosshair = self.mid_y + config["CROSSHAIRS_Y_OFFSET"]
                #Run img proc.
                circle = self.get_circle(img)
                # [x, y, r] 
                
                try:
                    self.Accumulator = np.append(self.Accumulator, [circle], axis=0)
                except ValueError:
                    self.Accumulator = np.empty((1,3))

                self.circle = np.uint16(np.around(np.average(self.Accumulator, axis=0)))
                
                if len(circle) < 3:
                    d = config["ACCEPTABLE_DISTANCE_FROM_CENTER"] + 100
                else:
                    d = self.get_distance([self.x_crosshair,self.y_crosshair], [self.circle[0], self.circle[1]])
                
                
                self.draw_croshairs(img, r=r, circle_color = config["INDICATOR_CIRCLE_CENTER_COLOUR"] if d < config["ACCEPTABLE_DISTANCE_FROM_CENTER"] 
                                              else config["INDICATOR_CIRCLE_NOT_CENTER_COLOUR"],  
                                    
                                    circle_thickness = config["INDICATOR_CIRCLE_THICKNESS"]+1 if d < config["ACCEPTABLE_DISTANCE_FROM_CENTER"] 
                                    else config["INDICATOR_CIRCLE_THICKNESS"], color=config["CROSSHAIRS_COLOR"]
                                    )
                
                if(len(circle)==3):
                    self.draw_circle(img, self.circle)
                    if config["DRAW_DIRECTION_INDICATOR"] and d > config["ACCEPTABLE_DISTANCE_FROM_CENTER"]:
                        ro = r - 20
                        x_u = int(ro * cos(atan2(self.circle[1] - self.y_crosshair ,self.circle[0] - self.x_crosshair))) + self.x_crosshair
                        y_u = int(ro * sin(atan2(self.circle[1]-self.y_crosshair, self.circle[0]-self.x_crosshair))) + self.y_crosshair
                        cv.circle(img, (x_u, y_u), 5, (255,255,255), -1)
                if config["DRAW_SCAN_ZONE_CIRCLES"]:
                    cv.circle(img, (self.x_crosshair,self.y_crosshair),config["MIN_CIRCLE_RADIUS"], (200,50,100), 2 )
                    cv.circle(img, (self.x_crosshair,self.y_crosshair),config["MAX_CIRCLE_RADIUS"], (100,50,200), 2 )
                color_frame = cv.cvtColor(img, cv.COLOR_BGR2RGB)
                h, w, ch = color_frame.shape
                
                self.updateFrame.emit(QImage(color_frame.data, w, h, ch*w, QImage.Format_RGB888))

                if len(self.Accumulator) > 10:
                    self.Accumulator = np.empty((1,3))
                    np.append(self.Accumulator, [circle], axis=0)

    def draw_croshairs(self, frame, r = None, color=(100,255,100), circle_color=(100,255,100), circle_thickness=2):
        height, width, _ = frame.shape
        if r is None:
            r = min(int(width * config["INDICATOR_CIRCLE_RADIUS_WIDTH_RATIO"]), int(height* config["INDICATOR_CIRCLE_RADIUS_WIDTH_RATIO"]))
        cv.circle(frame, (self.mid_x + config["INDICATOR_CIRCLE_X_OFFSET"], self.mid_y + config["INDICATOR_CIRCLE_Y_OFFSET"]), r, circle_color, circle_thickness)
        cv.line(frame, (self.x_crosshair, 0), (self.x_crosshair,height), color, 2)
        cv.line(frame, (0,self.y_crosshair),(width, self.y_crosshair), color, 2)

    def draw_circle(self, frame, circle):
        center = (circle[0], circle[1])
        # circle center
        cv.circle(frame, center, 5, (15, 15, 255), -1)
        # circle outline
        if config["DRAW_FASTENER_CIRCLE"]:
            radius = circle[2]
            cv.circle(frame, center, radius, (200, 200, 200 ), 3)
    


    def get_circle(self, frame):
        rows, cols, _ = frame.shape
        gray = cv.cvtColor(frame, cv.COLOR_RGB2GRAY)
        blur = cv.medianBlur(gray, config["MEDIAN_BLUR"])
        circles = cv.HoughCircles(blur, cv.HOUGH_GRADIENT, 1, rows / 16,
                           param1=100, param2=30,
                           minRadius= config["MIN_CIRCLE_RADIUS"], maxRadius= config["MAX_CIRCLE_RADIUS"])
        if circles is not None:
            return np.uint16(np.around(circles))[0][0]
        else:
            return []

    def get_distance(self, xy, circle_pos):
        return np.linalg.norm(np.array(xy)-np.array(circle_pos))


class FullscreenImageApp(QMainWindow):
    def __init__(self, screen_size):
        super().__init__()
        self.init_ui(screen_size)

    def init_ui(self, screen_size):
        self.setWindowTitle("Fullscreen Image Display")
        self.showFullScreen()
        # self.qImage = QImage(self)
        self.edit_circle = False
        self.edit_crosshairs = False
        self.edit_frame_pos = False
        self.edit_tolerance = False
        self.central_widget = QWidget(self)
        self.setCentralWidget(self.central_widget)
        self.text_timer = QTimer(self)
        self.text_timer.timeout.connect(self.clear_text)
        self.duration = 3000
        self.print_tolerance_text = False
        self.print_save_conf = False
        self.image_label = QLabel(self.central_widget)
        self.image_label.setGeometry(0,0,mainthread.nwidth,mainthread.nheight)
        
    def keyPressEvent(self, event):
        global config
        if event.key() in [Qt.Key_Left, Qt.Key_Up, Qt.Key_Right, Qt.Key_Down, Qt.Key_X, Qt.Key_C, Qt.Key_S, Qt.Key_F, Qt.Key_T, Qt.Key_Control, Qt.Key_R]:
            if event.isAutoRepeat():
                if event.key() == Qt.Key_Left:
                    if self.edit_crosshairs:
                        config["CROSSHAIRS_X_OFFSET"] -= 10
                    if self.edit_circle:
                        config["INDICATOR_CIRCLE_X_OFFSET"] -= 10
                    if self.edit_frame_pos:
                        config["FULLSCREEN_IMAGE_X_OFFSET"] -= 10
                    
                    print("Crosshairs X offset: {} , Y offset: {} || Circle X offset: {} , Y offset: {} || Frame X offset: {} , Y offset: {}".format(str(config["CROSSHAIRS_X_OFFSET"]), str(config["CROSSHAIRS_Y_OFFSET"]), 
                                                                               str(config["INDICATOR_CIRCLE_X_OFFSET"]), str(config["INDICATOR_CIRCLE_Y_OFFSET"]), 
                                                                               str(config["FULLSCREEN_IMAGE_X_OFFSET"]), str(config["FULLSCREEN_IMAGE_Y_OFFSET"])))
                if event.key() == Qt.Key_Right:
                    if self.edit_crosshairs:
                        config["CROSSHAIRS_X_OFFSET"] += 10
                    if self.edit_circle:
                        config["INDICATOR_CIRCLE_X_OFFSET"] += 10
                    if self.edit_frame_pos:
                        config["FULLSCREEN_IMAGE_X_OFFSET"] += 10
                    print("Crosshairs X offset: {} , Y offset: {} || Circle X offset: {} , Y offset: {} || Frame X offset: {} , Y offset: {}".format(str(config["CROSSHAIRS_X_OFFSET"]), str(config["CROSSHAIRS_Y_OFFSET"]), 
                                                                               str(config["INDICATOR_CIRCLE_X_OFFSET"]), str(config["INDICATOR_CIRCLE_Y_OFFSET"]), 
                                                                               str(config["FULLSCREEN_IMAGE_X_OFFSET"]), str(config["FULLSCREEN_IMAGE_Y_OFFSET"])))
                if event.key() == Qt.Key_Up:
                    if self.edit_crosshairs:
                        config["CROSSHAIRS_Y_OFFSET"] -= 10
                    if self.edit_circle:
                        config["INDICATOR_CIRCLE_Y_OFFSET"] -= 10
                    if self.edit_frame_pos:
                        config["FULLSCREEN_IMAGE_Y_OFFSET"] -= 10
                    if self.edit_tolerance:
                        config["ACCEPTABLE_DISTANCE_FROM_CENTER"] += 0.5
                        self.text_timer.start(self.duration)
                        self.print_tolerance_text = True

                    print("Crosshairs X offset: {} , Y offset: {} || Circle X offset: {} , Y offset: {} || Frame X offset: {} , Y offset: {}".format(str(config["CROSSHAIRS_X_OFFSET"]), str(config["CROSSHAIRS_Y_OFFSET"]), 
                                                                               str(config["INDICATOR_CIRCLE_X_OFFSET"]), str(config["INDICATOR_CIRCLE_Y_OFFSET"]), 
                                                                               str(config["FULLSCREEN_IMAGE_X_OFFSET"]), str(config["FULLSCREEN_IMAGE_Y_OFFSET"])))
                if event.key() == Qt.Key_Down:
                    if self.edit_crosshairs:
                        config["CROSSHAIRS_Y_OFFSET"] += 10
                    if self.edit_circle:
                        config["INDICATOR_CIRCLE_Y_OFFSET"] += 10
                    if self.edit_frame_pos:
                        config["FULLSCREEN_IMAGE_Y_OFFSET"] += 10
                    if self.edit_tolerance:
                        if config["ACCEPTABLE_DISTANCE_FROM_CENTER"] < 0:
                            config["ACCEPTABLE_DISTANCE_FROM_CENTER"] = 0
                        if config["ACCEPTABLE_DISTANCE_FROM_CENTER"] == 0:
                            self.text_timer.start(self.duration)
                            self.print_tolerance_text = True
                        else: 
                            config["ACCEPTABLE_DISTANCE_FROM_CENTER"] -= 0.5
                            self.text_timer.start(self.duration)
                            self.print_tolerance_text = True
                        
                    print("Crosshairs X offset: {} , Y offset: {} || Circle X offset: {} , Y offset: {} || Frame X offset: {} , Y offset: {}".format(str(config["CROSSHAIRS_X_OFFSET"]), str(config["CROSSHAIRS_Y_OFFSET"]), 
                                                                               str(config["INDICATOR_CIRCLE_X_OFFSET"]), str(config["INDICATOR_CIRCLE_Y_OFFSET"]), 
                                                                               str(config["FULLSCREEN_IMAGE_X_OFFSET"]), str(config["FULLSCREEN_IMAGE_Y_OFFSET"])))            
            else:
                if event.key() == Qt.Key_Left:
                    if self.edit_crosshairs:
                        config["CROSSHAIRS_X_OFFSET"] -= 1
                    if self.edit_circle:
                        config["INDICATOR_CIRCLE_X_OFFSET"] -= 1
                    if self.edit_frame_pos:
                        config["FULLSCREEN_IMAGE_X_OFFSET"] -= 1
                    # print("Left key once!")
                    
                    print("Crosshairs X offset: {} , Y offset: {} || Circle X offset: {} , Y offset: {} || Frame X offset: {} , Y offset: {}".format(str(config["CROSSHAIRS_X_OFFSET"]), str(config["CROSSHAIRS_Y_OFFSET"]), 
                                                                               str(config["INDICATOR_CIRCLE_X_OFFSET"]), str(config["INDICATOR_CIRCLE_Y_OFFSET"]), 
                                                                               str(config["FULLSCREEN_IMAGE_X_OFFSET"]), str(config["FULLSCREEN_IMAGE_Y_OFFSET"])))                
                if event.key() == Qt.Key_Right:
                    if self.edit_crosshairs:
                        config["CROSSHAIRS_X_OFFSET"] += 1
                    if self.edit_circle:
                        config["INDICATOR_CIRCLE_X_OFFSET"] += 1
                    if self.edit_frame_pos:
                        config["FULLSCREEN_IMAGE_X_OFFSET"] += 1
                        
                        
                    print("Crosshairs X offset: {} , Y offset: {} || Circle X offset: {} , Y offset: {} || Frame X offset: {} , Y offset: {}".format(str(config["CROSSHAIRS_X_OFFSET"]), str(config["CROSSHAIRS_Y_OFFSET"]), 
                                                                               str(config["INDICATOR_CIRCLE_X_OFFSET"]), str(config["INDICATOR_CIRCLE_Y_OFFSET"]), 
                                                                               str(config["FULLSCREEN_IMAGE_X_OFFSET"]), str(config["FULLSCREEN_IMAGE_Y_OFFSET"])))             
                if event.key() == Qt.Key_Up:
                    if self.edit_crosshairs:
                        config["CROSSHAIRS_Y_OFFSET"] -= 1
                    if self.edit_circle:
                        config["INDICATOR_CIRCLE_Y_OFFSET"] -= 1
                    if self.edit_frame_pos:
                        config["FULLSCREEN_IMAGE_Y_OFFSET"] -= 1
                    if self.edit_tolerance:
                        config["ACCEPTABLE_DISTANCE_FROM_CENTER"] += 0.1
                        self.text_timer.start(self.duration)
                        self.print_tolerance_text = True
                    
                    
                    print("Crosshairs X offset: {} , Y offset: {} || Circle X offset: {} , Y offset: {} || Frame X offset: {} , Y offset: {}".format(str(config["CROSSHAIRS_X_OFFSET"]), str(config["CROSSHAIRS_Y_OFFSET"]), 
                                                                               str(config["INDICATOR_CIRCLE_X_OFFSET"]), str(config["INDICATOR_CIRCLE_Y_OFFSET"]), 
                                                                               str(config["FULLSCREEN_IMAGE_X_OFFSET"]), str(config["FULLSCREEN_IMAGE_Y_OFFSET"])))                
                if event.key() == Qt.Key_Down:
                    if self.edit_crosshairs:
                        config["CROSSHAIRS_Y_OFFSET"] += 1
                    if self.edit_circle:
                        config["INDICATOR_CIRCLE_Y_OFFSET"] += 1
                    if self.edit_frame_pos:
                        config["FULLSCREEN_IMAGE_Y_OFFSET"] += 1
                    if self.edit_tolerance:
                        if config["ACCEPTABLE_DISTANCE_FROM_CENTER"] < 0:
                            config["ACCEPTABLE_DISTANCE_FROM_CENTER"] = 0
                        if config["ACCEPTABLE_DISTANCE_FROM_CENTER"] == 0:
                            self.text_timer.start(self.duration)
                            self.print_tolerance_text = True
                        else: 
                            config["ACCEPTABLE_DISTANCE_FROM_CENTER"] -= 0.1
                            self.text_timer.start(self.duration)
                            self.print_tolerance_text = True
                    
                    print("Crosshairs X offset: {} , Y offset: {} || Circle X offset: {} , Y offset: {} || Frame X offset: {} , Y offset: {}".format(str(config["CROSSHAIRS_X_OFFSET"]), str(config["CROSSHAIRS_Y_OFFSET"]), 
                                                                               str(config["INDICATOR_CIRCLE_X_OFFSET"]), str(config["INDICATOR_CIRCLE_Y_OFFSET"]), 
                                                                               str(config["FULLSCREEN_IMAGE_X_OFFSET"]), str(config["FULLSCREEN_IMAGE_Y_OFFSET"])))              
                
                if event.key() == Qt.Key_X:
                    self.edit_crosshairs = not self.edit_crosshairs
                    print("edit crosshairs: {}".format(str(self.edit_crosshairs)))
                    if self.edit_crosshairs:
                        config["CROSSHAIRS_COLOR"] = (150,150,150)
                    else:
                        config["CROSSHAIRS_COLOR"] = (0,0,0)

                if event.key() == Qt.Key_C:
                    self.edit_circle = not self.edit_circle
                    print("edit circle: {}".format(str(self.edit_circle)))
                    if self.edit_circle:
                        config["INDICATOR_CIRCLE_NOT_CENTER_COLOUR"] = (150,150,255)
                        config["INDICATOR_CIRCLE_CENTER_COLOUR"] = (150,255,150)
                    else:
                        config["INDICATOR_CIRCLE_NOT_CENTER_COLOUR"] = (50,50,255)
                        config["INDICATOR_CIRCLE_CENTER_COLOUR"] = (100,255,100)
                if event.key() == Qt.Key_F:
                    self.edit_frame_pos = not self.edit_frame_pos
                    print("edit frame pos:{}".format(str(self.edit_crosshairs)))
                
                if event.key() == Qt.Key_T:
                    self.edit_tolerance = not self.edit_tolerance
                    
                if event.key() == Qt.Key_S:
                    self.edit_circle = False
                    self.edit_crosshairs = False
                    config["CROSSHAIRS_COLOR"] = (0,0,0)
                    config["INDICATOR_CIRCLE_NOT_CENTER_COLOUR"] = (50,50,255)
                    config["INDICATOR_CIRCLE_CENTER_COLOUR"] = (100,255,100)
                    save_config()
                    self.print_save_conf = True
                    self.text_timer.start(self.duration)
                    print("Saved settings")
                if event.key() == Qt.Key_R and event.modifiers() & Qt.ControlModifier:
                    try:
                        reboot = str(subprocess.check_output(["whereis", "reboot"])).split()
                        subprocess.run([reboot[1]]) #, "r", "now"])
                    except Exception:
                        logging.error("reboot cmd not found")
                        print("reboot cmd not found")            # print("ctrl+r pressed")
    
    def print_tolerance_on_screen(self, qimage, duration=3000):
        self.duration = duration
        painter = QPainter(qimage)
        painter.setPen(QColor(255, 255, 255, 255))
        painter.drawText(mainthread.mid_x-140, mainthread.mid_y-30, "Tolerance: {:.2f} pixels".format(config["ACCEPTABLE_DISTANCE_FROM_CENTER"]))  # Modify position as needed
        painter.end()
    
    def print_save_confirmation_on_screen(self, qimage, duration=3000):
        self.duration = duration
        painter = QPainter(qimage)
        painter.setPen(QColor(255, 255, 255, 255))
        painter.setFont(QFont("Segoe UI", 16))
        painter.drawText(mainthread.mid_x-97, mainthread.mid_y-10, "Settings Saved")  # Modify position as needed
        painter.end()

    def clear_text(self):
        self.print_tolerance_text = False
        self.print_save_conf = False

    def reset_label_geometry(self):        
        self.image_label.setGeometry(config["FULLSCREEN_IMAGE_X_OFFSET"],config["FULLSCREEN_IMAGE_Y_OFFSET"],
                                     mainthread.nwidth*config["FULLSCREEN_IMAGE_RESIZE_MULTIPLIER"], 
                                     mainthread.nheight*config["FULLSCREEN_IMAGE_RESIZE_MULTIPLIER"])

    @Slot(QImage)
    def update_image(self, qimage:QImage):
        if self.print_tolerance_text:
            self.print_tolerance_on_screen(qimage)
        if self.print_save_conf:
            self.print_save_confirmation_on_screen(qimage, duration=1500)
        self.reset_label_geometry()
        pixmap = QPixmap.fromImage(qimage.scaled(self.image_label.geometry().size(),Qt.KeepAspectRatio))
        self.image_label.setPixmap(pixmap)
        self.image_label.setScaledContents(True)
    

if __name__ == "__main__":
    load_config()
    mainthread = MainImageProcThread()
    mainthread.start()
    app = QApplication(sys.argv)
    screen = app.primaryScreen()
    window = FullscreenImageApp(screen.availableGeometry())
    window.show()

    mainthread.updateFrame.connect(window.update_image)
    # # You can change this QImage with your own image loading logic
    # image = QImage("Screenshot.png")
    # # You should replace this line with the logic that updates your QImage
    # window.update_image(image)
    app.aboutToQuit.connect(mainthread.stop)
    sys.exit(app.exec())
