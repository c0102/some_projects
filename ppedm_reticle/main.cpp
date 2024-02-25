#include <stdio.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <stdio.h>

using namespace cv;
int main(int argc, char** argv )
{

    Mat src;
    std::vector<Vec3f> circles;
    //--- INITIALIZE VIDEOCAPTURE
    VideoCapture cap;
    // open the default camera using default API
    // cap.open(0);
    // OR advance usage: select any API backend
    int deviceID = 2;        // 0 = open default camera
    int apiID = cv::CAP_V4L; // 0 = autodetect default API
    // open selected camera using selected API
    cap.open(deviceID, apiID);
    // check if we succeeded
    if (!cap.isOpened())
    {
        std::cerr << "ERROR! Unable to open camera\n";
        return -1;
    }
    //--- GRAB AND WRITE LOOP
    std::cout << "Start grabbing" << std::endl
              << "Press any key to terminate" << std::endl;
    for (;;)
    {
        // wait for a new frame from camera and store it into 'frame'
        cap.read(src);
        // check if we succeeded
        if (src.empty())
        {
            std::cerr << "ERROR! blank frame grabbed\n";
            break;
        }
        // Mat dst;
        resize(src, src, Size(640,400), 0, 0, INTER_LINEAR);
        Mat gray;
        cvtColor(src, gray, COLOR_BGR2GRAY);
        medianBlur(gray, gray, 5);
        
        HoughCircles(gray, circles, HOUGH_GRADIENT, 1,
                     gray.rows / 16, // change this value to detect circles with different distances to each other
                     100, 30, 1, 60  // change the last two parameters
                                     // (min_radius & max_radius) to detect larger circles
        );
        for (size_t i = 0; i < circles.size(); i++)
        {
            Vec3i c = circles[i];
            Point center = Point(c[0], c[1]);
            // circle center
            circle(src, center, 1, Scalar(0, 100, 100), 3, LINE_AA);
            // circle outline
            int radius = c[2];
            circle(src, center, radius, Scalar(255, 0, 255), 3, LINE_AA);
        }
        // // show live and wait for a key with timeout long enough to show images
        flip(src, src, 0);
        imshow("Live", src);
        if (pollKey() >= 0)
            break;
    }

    return EXIT_SUCCESS;
}