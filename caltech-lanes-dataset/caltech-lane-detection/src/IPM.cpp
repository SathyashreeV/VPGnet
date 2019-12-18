/***
author Daniel Hsu
date 09/2019
to compile the project use the following command:
g++ IPM.cpp InversePerspectiveMapping.cc mcv.cc -o a `pkg-config --libs opencv`
***/

#include <opencv2/opencv.hpp>
#include <list>
#include <vector>
#include "InversePerspectiveMapping.hh"
#include "mcv.hh"
#include <cstring>
#include <fstream>
#include <iostream>

using namespace std;
using namespace cv;

#define UNITY 1
#include <chrono>

// For good performance, please use the overloaded function parse_config() with UNITY as a parameter
// to avoid opening the config file all the time.
// Remember to change all the parameters in parse_config() below to your desired parameters!
// Change focalLength will only affect the scale in ground coordinates, but will not affect the IPM'ed image itself! (since image can be scaled arbitrarily)

void parse_config(string filename, int &ipmWidth, int &ipmHeight, LaneDetector::CameraInfo* cameraInfo, LaneDetector::IPMInfo &ipmInfo, int options){
    // This reloaded function is to use the constant camera config for Unity
    // so that the program doesn't load the config file every time
    // in order to change parameters, use the other function!
    if (options == UNITY){
        // sizes: the output size, can be arbitrary
        ipmWidth = 160; // output size!
        ipmHeight = 120;

        // IPM info: define the pixel range
        ipmInfo.vpPortion = 0.045; // how far is the image top from vanishing point (bc vp is too far, can't display all), default 0.05
        ipmInfo.ipmLeft = 0;
        ipmInfo.ipmRight = 159;//639;
        ipmInfo.ipmTop = 12;
        ipmInfo.ipmBottom = 119;// 380;
        ipmInfo.ipmInterpolation = 0;

        // focal length
        float focalLengthX = 100;
        float focalLengthY = 100;
        cameraInfo->focalLength = cvPoint2D32f(focalLengthX, focalLengthY);
        // optical center coordinates in image frame (origin is (0,0) at top left)
        float opticalCenterX = 79;//317.9;
        float opticalCenterY = 59;//256.5;
        cameraInfo->opticalCenter = cvPoint2D32f(opticalCenterX, opticalCenterY);
        // height of the camera in mm
        cameraInfo->cameraHeight = 2000; // 2179.8; //# 393.7 + 1786.1
        // pitch of the camera
        cameraInfo->pitch = 0.0;//14.0 * CV_PI / 180.0; // in radius!
        // yaw of the camera
        cameraInfo->yaw  = 0.0 * CV_PI / 180.0;
        // imag width and height
        cameraInfo->imageWidth = 160; // camera photo size! input size.
        cameraInfo->imageHeight = 120;
    }
    return;
}

void parse_config(string filename, int &ipmWidth, int &ipmHeight, LaneDetector::CameraInfo* cameraInfo, LaneDetector::IPMInfo &ipmInfo){
    ifstream config_file;
    config_file.open(filename.c_str());
    string line, var_name;
    float number;
    struct{
        int ipmWidth, ipmHeight, ipmLeft, ipmRight, ipmTop, ipmBottom, ipmInterpolation, imageWidth, imageHeight;
        float vpPortion, focalLengthX, focalLengthY, opticalCenterX, opticalCenterY, cameraHeight, pitch, yaw;
    } configurations;
    while (getline(config_file, line)){
        if (line[0] != '#') // this line not commented
        {
            istringstream iss1(line);
            iss1 >> var_name >> number;
            if (var_name == "ipmWidth")
                configurations.ipmWidth = number;
            else if (var_name == "ipmHeight")
                configurations.ipmHeight = number;
            else if (var_name == "vpPortion")
                configurations.vpPortion = number;
            else if (var_name == "ipmLeft")
                configurations.ipmLeft = number;
            else if (var_name == "ipmRight")
                configurations.ipmRight = number;
            else if (var_name == "ipmTop")
                configurations.ipmTop = number;
            else if (var_name == "ipmBottom")
                configurations.ipmBottom = number;
            else if (var_name == "ipmInterpolation")
                configurations.ipmInterpolation = number;
            else if (var_name == "focalLengthX")
                configurations.focalLengthX = number;
            else if (var_name == "focalLengthY")
                configurations.focalLengthY = number;
            else if (var_name == "opticalCenterX")
                configurations.opticalCenterX = number;
            else if (var_name == "opticalCenterY")
                configurations.opticalCenterY = number;
            else if (var_name == "cameraHeight")
                configurations.cameraHeight = number;
            else if (var_name == "pitch")
                configurations.pitch = number;
            else if (var_name == "yaw")
                configurations.yaw = number;
            else if (var_name == "imageWidth")
                configurations.imageWidth = number;
            else if (var_name == "imageHeight")
                configurations.imageHeight = number;
        }
    }
    // sizes: the output size, can be arbitrary
    ipmWidth = configurations.ipmWidth; // output size!
    ipmHeight = configurations.ipmHeight;

    // IPM info: define the pixel range
    ipmInfo.vpPortion = configurations.vpPortion; // how far is the image top from vanishing point (bc vp is too far, can't display all)
    ipmInfo.ipmLeft = configurations.ipmLeft;
    ipmInfo.ipmRight = configurations.ipmRight;
    ipmInfo.ipmTop = configurations.ipmTop;
    ipmInfo.ipmBottom = configurations.ipmBottom;// 380;
    ipmInfo.ipmInterpolation = configurations.ipmInterpolation;

    // focal length
    float focalLengthX = configurations.focalLengthX;
    float focalLengthY = configurations.focalLengthY;
    cameraInfo->focalLength = cvPoint2D32f(focalLengthX, focalLengthY);
    // optical center coordinates in image frame (origin is (0,0) at top left)
    float opticalCenterX = configurations.opticalCenterX;
    float opticalCenterY = configurations.opticalCenterY;
    cameraInfo->opticalCenter = cvPoint2D32f(opticalCenterX, opticalCenterY);
    // height of the camera in mm
    cameraInfo->cameraHeight = configurations.cameraHeight; //# 393.7 + 1786.1
    // pitch of the camera
    cameraInfo->pitch = configurations.pitch * CV_PI / 180.0; // in radius!
    // yaw of the camera
    cameraInfo->yaw  = configurations.yaw * CV_PI / 180.0;
    // imag width and height
    cameraInfo->imageWidth = configurations.imageWidth; // camera photo size! input size.
    cameraInfo->imageHeight = configurations.imageHeight;
    
    config_file.close();
    return;
}

struct scale_xy{
    double step_x, step_y, xfMax, xfMin, yfMax, yfMin;
    int ipmWidth, ipmHeight;
};

double _xfMax, _xfMin, _yfMax, _yfMin; // global var for use in other functions
scale_xy get_resize_scale(int width, int height, LaneDetector::IPMInfo* ipmInfo, LaneDetector::CameraInfo* cameraInfo){
    // find the scale between ground coord and ipm image (dim_ground / scale = dim_ipm)
    // x in ground = ((x in ipm - ipmWidth / 2) * step_x + (_xfMax + _xfMin) / 2.0)
    // y in ground = ((ipmHeight - y in ipm) * step_y + _yfMin)

    FLOAT u, v;
    v = height;
    u = width;
    //get the vanishing point
    FLOAT_POINT2D vp;
    vp = mcvGetVanishingPoint(cameraInfo);
    vp.y = MAX(0, vp.y);

    FLOAT_MAT_ELEM_TYPE eps = ipmInfo->vpPortion * v;//VP_PORTION*v;
    ipmInfo->ipmLeft = MAX(0, ipmInfo->ipmLeft);
    ipmInfo->ipmRight = MIN(u-1, ipmInfo->ipmRight);
    ipmInfo->ipmTop = MAX(vp.y+eps, ipmInfo->ipmTop);
    ipmInfo->ipmBottom = MIN(v-1, ipmInfo->ipmBottom);
    FLOAT_MAT_ELEM_TYPE uvLimitsp[] = {vp.x,
    ipmInfo->ipmRight, ipmInfo->ipmLeft, vp.x,
    ipmInfo->ipmTop, ipmInfo->ipmTop,   ipmInfo->ipmTop,  ipmInfo->ipmBottom};
    //{vp.x, u, 0, vp.x,
    //vp.y+eps, vp.y+eps, vp.y+eps, v};
    CvMat uvLimits = cvMat(2, 4, FLOAT_MAT_TYPE, uvLimitsp);

    //get these points on the ground plane
    CvMat * xyLimitsp = cvCreateMat(2, 4, FLOAT_MAT_TYPE);
    CvMat xyLimits = *xyLimitsp;
    mcvTransformImage2Ground(&uvLimits, &xyLimits, cameraInfo);
    CvMat row1, row2;
    cvGetRow(&xyLimits, &row1, 0);
    cvGetRow(&xyLimits, &row2, 1);
    double xfMax, xfMin, yfMax, yfMin;
    cvMinMaxLoc(&row1, (double*)&xfMin, (double*)&xfMax, 0, 0, 0);
    cvMinMaxLoc(&row2, (double*)&yfMin, (double*)&yfMax, 0, 0, 0);
    _xfMax = xfMax;
    _xfMin = xfMin;
    _yfMax = yfMax;
    _yfMin = yfMin;

    // cout<<"ymin: "<<yfMin<<" ymax:"<<yfMax <<endl; // Rui
    INT outRow = height;
    INT outCol = width;
    FLOAT_MAT_ELEM_TYPE stepRow = (yfMax-yfMin)/outRow;
    FLOAT_MAT_ELEM_TYPE stepCol = (xfMax-xfMin)/outCol;

    scale_xy scale;
    scale.step_x = stepCol;
    scale.step_y = stepRow;
    scale.xfMax = xfMax;
    scale.xfMin = xfMin;
    scale.yfMax = yfMax;
    scale.yfMin = yfMin;
    scale.ipmWidth = width;
    scale.ipmHeight = height;

    return scale;
}

scale_xy points_image2ground(int n, float *points_x, int m, float *points_y){ // n == m is the number of points, for python interface

    LaneDetector::CameraInfo* cameraInfo = new LaneDetector::CameraInfo();
    LaneDetector::IPMInfo ipmInfo;
    int ipmWidth = 640; // default, to be changed by parse_config function
    int ipmHeight = 480;
    string filename = "camera.conf";
    parse_config(filename, ipmWidth, ipmHeight, cameraInfo, ipmInfo); // get rid of "UNITY" if you want to use config in file
    //parse_config(filename, ipmWidth, ipmHeight, cameraInfo, ipmInfo, UNITY); // get rid of "UNITY" if you want to use config in file

    // FLOAT_MAT_ELEM_TYPE uv[] = {pt1.x, pt2.x, pt1.y, pt2.y};
    FLOAT_MAT_ELEM_TYPE uv[2 * n];
    for (int i = 0; i < n; i ++)
    {
        uv[i] = points_x[i]; // change the format of points!
        uv[n + i] = points_y[i];
    }
    CvMat uv_cvmat = cvMat(2, n, FLOAT_MAT_TYPE, uv);
    CvMat * xy = cvCreateMat(2, n, FLOAT_MAT_TYPE);
    CvMat xy_cvmat = *xy;
    mcvTransformImage2Ground(&uv_cvmat, &xy_cvmat, cameraInfo);
    for (int i = 0; i < n; i ++)
    {
        points_x[i] = CV_MAT_ELEM(xy_cvmat, float, 0, i);
        points_y[i] = CV_MAT_ELEM(xy_cvmat, float, 1, i);
    }
    return get_resize_scale(ipmWidth, ipmHeight, &ipmInfo, cameraInfo);
}

scale_xy points_ipm2image(int n, float *points_x, int m, float *points_y){ // n == m is the number of points, for python interface
// NOTICE! this is not from ground to image, but from ipm image back to driver's perspective image!
    LaneDetector::CameraInfo* cameraInfo = new LaneDetector::CameraInfo();
    LaneDetector::IPMInfo ipmInfo;
    int ipmWidth = 640; // default, to be changed by parse_config function
    int ipmHeight = 480;
    string filename = "camera.conf";
    
    parse_config(filename, ipmWidth, ipmHeight, cameraInfo, ipmInfo);
    //parse_config(filename, ipmWidth, ipmHeight, cameraInfo, ipmInfo, UNITY); // get rid of "UNITY" if you want to use config in file

    scale_xy step_size = get_resize_scale(ipmWidth, ipmHeight, &ipmInfo, cameraInfo);

    // FLOAT_MAT_ELEM_TYPE uv[] = {pt1.x, pt2.x, pt1.y, pt2.y};
    FLOAT_MAT_ELEM_TYPE xy[2 * n];
    for (int i = 0; i < n; i ++)
    {
        xy[i] = static_cast<int> ((points_x[i] - ipmWidth / 2) * step_size.step_x + (_xfMax + _xfMin) / 2.0);
        xy[n + i] = static_cast<int> ((ipmHeight - points_y[i]) * step_size.step_y + _yfMin);
    }
    CvMat xy_cvmat = cvMat(2, n, FLOAT_MAT_TYPE, xy);
    CvMat * uv = cvCreateMat(2, n, FLOAT_MAT_TYPE);
    CvMat uv_cvmat = *uv;
    mcvTransformGround2Image(&xy_cvmat, &uv_cvmat, cameraInfo);
    for (int i = 0; i < n; i ++)
    {
        points_x[i] = CV_MAT_ELEM(uv_cvmat, float, 0, i);
        points_y[i] = CV_MAT_ELEM(uv_cvmat, float, 1, i);
    }
    return step_size;
}

scale_xy image_ipm(float *input, int h_in, int w_in, float *output, int h, int w){ // convert image to ipm'ed image

    CvMat converted_img = cvMat(h_in, w_in, FLOAT_MAT_TYPE, input); // gray image

    Mat debug_img = cvarrToMat(&converted_img);
    CvMat *int_image = &converted_img;
    CvMat *inImage = cvCreateMat(int_image->height, int_image->width, FLOAT_MAT_TYPE);
    cvConvertScale(int_image, inImage, 1./255);

    Mat output_img = cvarrToMat(&converted_img);
    // imwrite("gray.png", output_img);

    LaneDetector::CameraInfo *cameraInfo = new LaneDetector::CameraInfo();
    LaneDetector::IPMInfo ipmInfo;
    int ipmWidth = 640; // default, can be changed by function
    int ipmHeight = 480;
    string filename = "camera.conf";

    parse_config(filename, ipmWidth, ipmHeight, cameraInfo, ipmInfo);
    //parse_config(filename, ipmWidth, ipmHeight, cameraInfo, ipmInfo, UNITY); // get rid of "UNITY" if you want to use config in file

    CvMat * ipm = cvCreateMat(ipmHeight, ipmWidth, inImage->type); // the picture after IPM will be stored in ipm
    // execute GetIPM, new image is ipm
    list<CvPoint> outPixels;

    //auto t1 = chrono::high_resolution_clock::now();
    LaneDetector::mcvGetIPM(inImage, ipm, &ipmInfo, cameraInfo);
    // LaneDetector::SHOW_IMAGE(ipm, "IPM_image");
    cvConvertScale(ipm, ipm, 255);

    output_img = cvarrToMat(ipm);

    int minj = static_cast<int>(output_img.cols * 0.25), maxj = static_cast<int>(output_img.cols * 0.75); // Rui - focus on the central image only
    for (int i = 0; i < output_img.rows; i++)
        for (int j = minj; j < maxj; j++)
        {
            // cout << output_img.at<float>(i, j) << " "; // debug
            output[i * w + j] = (output_img.at<float>(i, j));
        }

    //auto t2 = chrono::high_resolution_clock::now();
    //cout << "TIME for XXX:" << chrono::duration_cast<chrono::milliseconds>(t2-t1).count() << "ms" << endl;

    return get_resize_scale(ipmWidth, ipmHeight, &ipmInfo, cameraInfo);
}

string type2str(int type) { // get image type
  string r;

  uchar depth = type & CV_MAT_DEPTH_MASK;
  uchar chans = 1 + (type >> CV_CN_SHIFT);

  switch ( depth ) {
    case CV_8U:  r = "8U"; break;
    case CV_8S:  r = "8S"; break;
    case CV_16U: r = "16U"; break;
    case CV_16S: r = "16S"; break;
    case CV_32S: r = "32S"; break;
    case CV_32F: r = "32F"; break;
    case CV_64F: r = "64F"; break;
    default:     r = "User"; break;
  }

  r += "C";
  r += (chans+'0');

  return r;
}

int main(){

    Mat image = imread("input.png");
    Mat gray_img;
    cvtColor(image, gray_img, COLOR_BGR2GRAY);
    CvMat converted_img = CvMat(gray_img);


    CvMat *int_image = &converted_img;
    CvMat *inImage = cvCreateMat(int_image->height, int_image->width, FLOAT_MAT_TYPE);
    cvConvertScale(int_image, inImage, 1./255);

    Mat output_img = cvarrToMat(&converted_img);
    imwrite("gray.png", output_img);

    LaneDetector::CameraInfo *cameraInfo = new LaneDetector::CameraInfo();
    LaneDetector::IPMInfo ipmInfo;
    int ipmWidth = 640; // default, can be changed by function
    int ipmHeight = 480;
    string filename = "camera.conf";

    parse_config(filename, ipmWidth, ipmHeight, cameraInfo, ipmInfo);

    CvMat * ipm = cvCreateMat(ipmHeight, ipmWidth, inImage->type); // the picture after IPM will be stored in ipm
    // execute GetIPM, new image is ipm
    list<CvPoint> outPixels;
    LaneDetector::mcvGetIPM(inImage, ipm, &ipmInfo, cameraInfo);
    //printf("Press any key to continue!\n");
    //LaneDetector::SHOW_IMAGE(ipm, "IPM_image");
    cvConvertScale(ipm, ipm, 255); // 0-255, still float
    output_img = cvarrToMat(ipm);

    cv::imwrite("output.png", output_img);

    return 0;
}
