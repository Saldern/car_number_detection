#include "regiondetector.h"
#include <time.h>

bool RegionDetector::verifySizes(RotatedRect mr){  
    float error = 0.4; 
    //Car plate size: 52x11 aspect 4,7272 =======TODO: change size=====
    float aspect = 4.7272; 
    //Set a min and max roi. All other patchs are discarded 
    int min = 15*aspect*15; // minimum roi 
    int max = 125*aspect*125; // maximum roi 
    //Get only patchs that match to a respect ratio. 
    float rmin = aspect-aspect*error; 
    float rmax = aspect+aspect*error; 

    int roi = mr.size.height * mr.size.width; 
    float k = (float)mr.size.width / (float)mr.size.height; 
    if(k<1)
        k = (float)mr.size.height / (float)mr.size.width; 
 
    if(( roi < min || roi > max ) || ( k < rmin || k > rmax )){ 
        return false; 
    }else{
        return true; 
    } 
} 
Mat RegionDetector::histeq(Mat input) 
{ 
    Mat out(input.size(), input.type()); 
    if(input.channels()==3){ 
        Mat hsv; 
        vector<Mat> hsvSplit; 
        cvtColor(input, hsv, CV_BGR2HSV); 
        split(hsv, hsvSplit); 
        equalizeHist(hsvSplit[2], hsvSplit[2]); 
        merge(hsvSplit, hsv); 
        cvtColor(hsv, out, CV_HSV2BGR); 
    }else if(input.channels()==1){ 
        equalizeHist(input, out); 
    }  
    return out;  
} 


RegionDetector::RegionDetector(void)
{
}
void RegionDetector::threshold(Mat src)
{
	//convert to gray
	Mat img_gray;
	cv::cvtColor(src,img_gray, CV_BGR2GRAY);
	blur(img_gray, img_gray, Size(5,5));
	imshow("blur", img_gray);

    //find first horizontal derivative( vertical lines which car plate usually has)
	Mat img_sobel;
	Sobel(img_gray, img_sobel, CV_8U, 1,0);
	imshow("sobel", img_sobel);

    //threshold
	cv::threshold(img_sobel,img_temp, 0, 255, CV_THRESH_OTSU+CV_THRESH_BINARY);
    imshow("50", img_temp);
}
void RegionDetector::close()
{
	//apply morphological close
	Mat element = getStructuringElement(MORPH_RECT, Size(17,3));
	morphologyEx(img_temp, img_temp, CV_MOP_CLOSE, element);
    imshow("close", img_temp);
}
void RegionDetector::getMask()
{
    //find contours
	vector <vector<Point> > contours;
    cv::findContours(img_temp,contours,
	                    CV_RETR_EXTERNAL,	//retrive external contours
                        CV_CHAIN_APPROX_NONE);	// all pixels of each contour
    //extract rectangle of minimal area
    //Start to iterate to each contour found
    vector <vector<Point> >::iterator itc = contours.begin();
    vector<RotatedRect> rects;
					
    //Remove patch that has no inside limits of aspect ratio and area
    while (itc!=contours.end())
    {
    //create bounding rect of object
	    RotatedRect rr = minAreaRect(Mat(*itc));
	    if (!verifySizes(rr)){
		    itc = contours.erase(itc);
	    }else{
            std::cout<<"bene";
		    itc++;
		    rects.push_back(rr);
	    }
    }
    std::cout << rects.size();
    // Draw blue contours on a white image 
    Mat result; 
    img_temp.copyTo(result); 
    cv::drawContours(result,contours, 
            -1, // draw all contours 
             cv::Scalar(255,0,0), // in blue 
             1); // with a thickness of 1
    imshow("nn",result);
    for(int i=0; i< rects.size(); i++)
    { 
        std::cout<<"true\n";
        //For better rect cropping for each posible box 
        //Make floodfill algorithm because the plate has white background 
        //And then we can retrieve more clearly the contour box 
        circle(result, rects[i].center, 3, Scalar(0,255,0), -1); 
        //get the min size between width and height 
        float minSize=(rects[i].size.width < rects[i].size.height)?rects[i].size.width:rects[i].size.height; 
        minSize=minSize-minSize*0.5; 
        //initialize rand and get 5 points around center for floodfill algorithm 
        srand ( time(NULL) ); 
        //Initialize floodfill parameters and variables 
        Mat mask; 
        mask.create(img_temp.rows + 2, img_temp.cols + 2, CV_8UC1); 
        mask= Scalar::all(0); 
        int loDiff = 30; 
        int upDiff = 30; 
        int connectivity = 4; 
        int newMaskVal = 255; 
        int NumSeeds = 10; 
        Rect ccomp; 
        int flags = connectivity + (newMaskVal << 8 ) + CV_FLOODFILL_FIXED_RANGE + CV_FLOODFILL_MASK_ONLY; 
        for(int j=0; j<NumSeeds; j++)
        { 
            Point seed; 
            seed.x=rects[i].center.x+rand()%(int)minSize-(minSize/2); 
            seed.y=rects[i].center.y+rand()%(int)minSize-(minSize/2); 
            circle(result, seed, 1, Scalar(0,255,255), -1); 
            int area = floodFill(img_temp, mask, seed, Scalar(255,0,0), &ccomp, Scalar(loDiff, loDiff, loDiff), Scalar(upDiff, upDiff, upDiff), flags); 
        }
        std::cout<<"true";
        imshow("MASK", mask); 
        //Check new floodfill mask match for a correct patch. 
        //Get all points detected for get Minimal rotated Rect 
        vector<Point> pointsInterest; 
        Mat_<uchar>::iterator itMask = mask.begin<uchar>(); 
        Mat_<uchar>::iterator end = mask.end<uchar>(); 
        while(itMask!=end)
        {
            if(*itMask==255) 
                pointsInterest.push_back(itMask.pos()); 
            itMask++;
        }
        RotatedRect minRect = minAreaRect(pointsInterest);  
        if(verifySizes(minRect))
        {
            // rotated rectangle drawing  
            Point2f rect_points[4]; minRect.points( rect_points ); 
            for( int j = 0; j < 4; j++ ) 
                line( result, rect_points[j], rect_points[(j+1)%4], Scalar(0,0,255), 1, 8 );     

            //Get rotation matrix 
            float r = (float)minRect.size.width / (float)minRect.size.height; 
            float angle = minRect.angle;     
            if(r<1) 
                angle = 90+angle; 
            Mat rotmat = getRotationMatrix2D(minRect.center, angle,1); 
 
            //Create and rotate image 
            Mat img_rotated; 
            warpAffine(img_temp, img_rotated, rotmat, img_temp.size(), CV_INTER_CUBIC); 
            imshow("Contours", img_rotated);
            imshow("Contours", img_temp); 

            //Crop image 
            Size rect_size=minRect.size; 
            if(r < 1) 
                std::swap(rect_size.width, rect_size.height); 
            Mat img_crop; 
            getRectSubPix(img_rotated, rect_size, minRect.center, img_crop); 
            imshow("Contours", img_rotated);  
            
            Mat resultResized; 
            resultResized.create(33,144, CV_8UC3); 
            resize(img_crop, resultResized, resultResized.size(), 0, 0, INTER_CUBIC); 
            imshow("rs", resultResized);

            //Equalize croped image 
            Mat grayResult; 
            cvtColor(resultResized, grayResult, CV_BGR2GRAY);  
            blur(grayResult, grayResult, Size(3,3)); 
            grayResult=histeq(grayResult); 
            //return output;
            imshow("Contours", grayResult); 
        } 
    }        
 

}

