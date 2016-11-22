#include <iostream>
#include <omp.h>
#include <cmath>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <vector>
#include <numeric>
using namespace std;
using namespace cv;

int xGradient(Mat image, int x, int y)
{
return image.at<uchar>(y-1, x-1) +
            2*image.at<uchar>(y, x-1) +
             image.at<uchar>(y+1, x-1) -
              image.at<uchar>(y-1, x+1) -
               2*image.at<uchar>(y, x+1) -
                image.at<uchar>(y+1, x+1);
}
int yGradient(Mat image, int x, int y)
{
    return image.at<uchar>(y-1, x-1) +
            2*image.at<uchar>(y-1, x) +
             image.at<uchar>(y-1, x+1) -
              image.at<uchar>(y+1, x-1) -
               2*image.at<uchar>(y+1, x) -
                image.at<uchar>(y+1, x+1);
}

/**
  * Calculates the 2d zero-mean gaussian with the given x, y and standard deviation.
 **/
float normal_pdf_2d(int x, int y, float standard_deviation) 
{
    static const float inverted_2pi = 0.15915494309189535;
    float exp = (x*x+y*y)/(2*standard_deviation*standard_deviation);
    return inverted_2pi / (standard_deviation * standard_deviation)
           * std::exp(-exp);
}

/**
  * Applies a hard coded blurr with filter size 3 at the given x and y in IMAGE.
  * Returns the blurred value to place at x, y im IMAGE.
 **/
int blurr_filter_3 (Mat image, int x, int y, float standard_deviation)
{
    return normal_pdf_2d(-1, -1, standard_deviation) * image.at<uchar>(y-1, x-1) +
           normal_pdf_2d(-1, 0, standard_deviation) * image.at<uchar>(y, x-1) +
           normal_pdf_2d(-1, 1, standard_deviation) * image.at<uchar>(y+1, x-1) +
           normal_pdf_2d(0, -1, standard_deviation) * image.at<uchar>(y-1, x) +
           normal_pdf_2d(0, 0, standard_deviation) * image.at<uchar>(y, x) +
           normal_pdf_2d(0, 1, standard_deviation) * image.at<uchar>(y+1, x) +
           normal_pdf_2d(1, -1, standard_deviation) * image.at<uchar>(y-1, x+1) +
           normal_pdf_2d(1, 0, standard_deviation) * image.at<uchar>(y, x+1) +
           normal_pdf_2d(1, 1, standard_deviation) * image.at<uchar>(y+1, x+1);
}

/**
  * Generalized blurr with the given filter size and standard deviation.
 **/
int blurr (Mat image, int x, int y, int filter_size, float standard_deviation)
{
    float sum = 0.f;
    int i, j;
    for (i = -filter_size; i <= filter_size; i++) {
        for (j = -filter_size; j <= filter_size; j++) {
          sum += normal_pdf_2d(i, j, standard_deviation) * image.at<uchar>(y+j, x+i);
        }
    }
    return int(sum);
}

int blurr_image (Mat &image, Mat &dst, int filter_size, int margin)
{
    #pragma omp parallel for
    for (int y = 0; y < dst.rows; y++)
        for (int x = 0; x < dst.cols; x++)
            dst.at<uchar>(y, x) = 0;
    #pragma omp parallel for
    for (int y = filter_size + margin; y < image.rows - filter_size - margin; y++) 
        for (int x = filter_size + margin; x < image.cols - filter_size - margin; x++)
        {
            int blurred = blurr (image, x, y, filter_size, 1.0f);
            blurred = blurred > 255 ? 255 : blurred;
            blurred = blurred < 0 ? 0 : blurred;
            dst.at<uchar>(y, x) = blurred;
        }
}

// 1 for success, 0 for fail
int grayscale(Mat &src, Mat &grey, Mat &dst) {
  double start, end;
  start = omp_get_wtime();
  int gx, gy, sum;
  // TODO: replace this so it reads the specific image rather than a hardcoded one
  src= imread("example.png");  
  cvtColor(src,grey,CV_BGR2GRAY);
  dst = grey.clone();
  if( !grey.data )
  { 
    return 0; 
  }
  return 1;
}

/** 
 *  Applies a Sobel filter to a greyscale image in GREY and writes the result values
 *  to DST.
 **/
int sobel(Mat &grey, Mat &dst) {
  #pragma omp parallel for  
  for(int y = 0; y < grey.rows; y++)
    for(int x = 0; x < grey.cols; x++)
      dst.at<uchar>(y,x) = 0;  
  #pragma omp parallel for
  for(int y = 1; y < grey.rows - 1; y++){
    for(int x = 1; x < grey.cols - 1; x++){
      int gx = xGradient(grey, x, y);
      int gy = yGradient(grey, x, y);
      int sum = abs(gx) - abs(gy);
      sum = sum > 255 ? 255:sum;
      sum = sum < 0 ? 0 : sum;
      dst.at<uchar>(y,x) = sum;
    }
  }
}

int sobel_add(Mat &grey, Mat &dst) {
  #pragma omp parallel for  
  for(int y = 0; y < grey.rows; y++)
    for(int x = 0; x < grey.cols; x++)
      dst.at<uchar>(y,x) = 0;  
  #pragma omp parallel for
  for(int y = 1; y < grey.rows - 1; y++){
    for(int x = 1; x < grey.cols - 1; x++){
      int gx = xGradient(grey, x, y);
      int gy = yGradient(grey, x, y);
      int sum = abs(gy)*abs(gy) + abs(gx)*abs(gx);
      sum = sum > 255 ? 255:sum;
      sum = sum < 0 ? 0 : sum;
      dst.at<uchar>(y,x) = sum;
    }
  }
}

// Takes greyscaled picture, thresholds it and puts it dst
// In the python file, they use 225 for threshold_val and 255 for max_val
int threshold(Mat grey, Mat &dst, int threshold_val, int max_val) {
  #pragma omp parallel for  
  for(int y = 0; y < grey.rows; y++)
    for(int x = 0; x < grey.cols; x++)
      dst.at<uchar>(y,x) = 0;  
  #pragma omp parallel for
  for (int y = 0; y < grey.rows-1; y++) {
    for (int x = 0; x < grey.cols-1; x++) {
      // cout << "Thresholding at: " << x << ", " << y << "\n";
      if (grey.at<uchar>(y, x) >= threshold_val) {
        dst.at<uchar>(y, x) = max_val;
      } else {
        dst.at<uchar>(y, x) = 0;
      }
    }
  }
  // cout << "Done thresholding\n";
}

// Grayscales and runs sobel operation
/**  @function Erosion
  *  @Mat src -> source image Matrix
  *  @Mat dst -> pointer to destination Matrix
  *  @int erosion_elem -> 0 = rect, 1 = cross, 2 = ellipse
  *  @int erosion_size -> represents size of erosion around anchor point
  */
void Erosion(Mat& src, Mat& dst, int erosion_elem, int erosion_size)
{
  int erosion_type;
  if( erosion_elem == 0 ){ erosion_type = MORPH_RECT; }
  else if( erosion_elem == 1 ){ erosion_type = MORPH_CROSS; }
  else if( erosion_elem == 2) { erosion_type = MORPH_ELLIPSE; }

  Mat element = getStructuringElement( erosion_type,
                                       Size( 2*erosion_size + 1, 2*erosion_size+1 ),
                                       Point( erosion_size, erosion_size ) );

  /// Apply the erosion operation
  erode( src, dst, element );
}

/** @function Dilation 
  * @Mat src -> source image Matrix
  * @Mat dst -> pointer to destination Matrix
  * @int dilation_elem -> 0 = rect, 1 = cross, 2 = ellipse
  * @int dilation_size -> represents size of dilation around anchor point
  */
void Dilation(Mat& src, Mat& dst, int dilation_elem, int dilation_size)
{
  int dilation_type;
  if( dilation_elem == 0 ){ dilation_type = MORPH_RECT; }
  else if( dilation_elem == 1 ){ dilation_type = MORPH_CROSS; }
  else if( dilation_elem == 2) { dilation_type = MORPH_ELLIPSE; }

  Mat element = getStructuringElement( dilation_type,
                                       Size( 2*dilation_size + 1, 2*dilation_size+1 ),
                                       Point( dilation_size, dilation_size ) );
  /// Apply the dilation operation
  dilate( src, dst, element );
}

void multByElem(Mat& in1, Mat& in2, Mat& result){
  #pragma omp for
  for(int y = 0; y < in1.rows; y++){
    for(int x = 0; x < in1.cols; x++){
      result.at<uchar>(y, x) = in1.at<uchar>(y, x) * in2.at<uchar>(y, x);
    }
  }
}

void argMaxX(Mat in, int (&results)[2], int threshold) 
{
  results[0] = 0;
  results[1] = 0;
  int sums[in.rows];
  int maxIndex = 0;
  int secondMaxIndex = 0;
  #pragma omp for
  for (int y = 0; y < in.rows; y++)
    sums[y] = 0;
  #pragma omp for
  for (int y = 0; y < in.rows; y++)
        for (int x = 0; x < in.cols; x++)
            if (in.at<uchar>(y, x) > threshold) {
              sums[y] += 1;
            }
            // sums[y] += in.at<uchar>(y, x);
            // sums[y] += 1;

  #pragma omp for
  for (int z = 0; z < in.rows; z++) {
    if (abs(z - maxIndex) <= 5 || abs(z - secondMaxIndex) <= 5)
      continue;
    if (sums[z] > results[1]) {
      #pragma omp critical
      if (sums[z] > results[0]) {
        results[1] = results[0];
        results[0] = sums[z];
        secondMaxIndex = maxIndex;
        maxIndex = z;
      } else {
        results[1] = sums[z];
        secondMaxIndex = z;
      }
    }
  }
  results[0] = maxIndex;
  results[1] = secondMaxIndex;
}

void argMaxY(Mat in, int (&results)[2], int threshold)
{
  results[0] = 0;
  results[1] = 0;
  int sums[in.cols];
  int maxIndex = 0;
  int secondMaxIndex = 0;
  #pragma omp for
  for (int x = 0; x < in.cols; x++)
    sums[x] = 0;
  #pragma omp for
  for (int x = 0; x < in.cols; x++)
    for (int y = 0; y < in.rows; y++){
      if (in.at<uchar>(y, x) > threshold) {
        sums[x] += 1;
      }
      // sums[x] += in.at<uchar>(y, x);
      // cout << (int)in.at<uchar>(y, x) << "\n";
      // sums[x] += 1;
    }

  #pragma omp for
  for (int z = 0; z < in.cols; z++) {
    if (abs(z - maxIndex) <= 5 || abs(z - secondMaxIndex) <= 5)
      continue;
    if (sums[z] > results[1]) {
      #pragma omp critical
      if (sums[z] > results[0]) {
        results[1] = results[0];
        results[0] = sums[z];
        secondMaxIndex = maxIndex;
        maxIndex = z;
      } else {
        results[1] = sums[z];
        secondMaxIndex = z;
      }
    }
  }
  results[0] = maxIndex;
  results[1] = secondMaxIndex;
}

void drawLines(Mat in, Mat &out, int * xCoords, int * yCoords) {
  #pragma omp for
  for (int y = 0; y < in.rows; y++) {
    for (int x = 0; x < in.cols; x++) {
      if (x == xCoords[0] || x == xCoords[1] || y == yCoords[0] || y == yCoords[1]){
        out.at<uchar>(y, x) = 255;
      } else {
        out.at<uchar>(y, x) = 0;
      }
    }
  }
}

// Grayscales and runs sobel operation
int main()
{
  Mat src, grey, dst, blurred, blurred_again, thresholded, thresholded2, eroded,
      eroded2, dilated, dilated2, edge2;

  Mat sobel1, sobel2, sobel3, sobel4;
  Mat blur1, blur2, blur3, blur4;
  double start, end;
  start = omp_get_wtime();

  if (!grayscale(src, grey, dst)) {
    return -1;
  }

  blur1 = grey.clone();  
  blur2 = grey.clone();  
  blur3 = grey.clone();
  blur4 = grey.clone();
  sobel1 = grey.clone();
  sobel2 = grey.clone();
  sobel3 = grey.clone();
  sobel4 = grey.clone();
  Mat temp = grey.clone();

  blurr_image(grey, blur1, 3, 0);
  sobel(blur1, temp);
  threshold(temp, sobel1, 225, 255);\

  blurr_image(blur1, blur2, 9, 3);
  sobel(blur2, temp);
  threshold(temp, sobel2, 225, 255);

  blurr_image(sobel2, temp, 3, 12);
  threshold(temp, sobel2, 150, 255);

  // namedWindow("sobel2");
  // imshow("sobel2", sobel2);

  blurred = dst.clone();
  blurred_again = dst.clone();
  thresholded = dst.clone();
  thresholded2 = dst.clone();
  dilated = dst.clone();
  dilated2 = dst.clone();
  eroded = dst.clone();
  eroded2 = dst.clone();
  edge2 = dst.clone();

  // blurr_image (sobel2, blurred, 5);
  // threshold(blurred, thresholded, 210, 255);
  // blurr_image(thresholded, blurred, 5);
  // threshold(blurred, thresholded, 210, 255);

  // Erosion(sobel2, temp, 0, 1);

  // namedWindow("eroded");
  // imshow("eroded", temp);

  Dilation(sobel2, dilated, 0, 10);
  Dilation(dilated, dilated2, 0, 10);
  // Dilation(dilated2, dilated, 0, 10);
  // Dilation(dilated, dilated2, 0, 1);

  // namedWindow("dilated");
  // imshow("dilated", dilated2);

  sobel_add(dilated2, temp);
  blurr_image(temp, dst, 15, 15);

  // namedWindow("sobel");
  // imshow("sobel", dst);

  int vertical[2];
  int horizontal[2];

  argMaxX(dst, horizontal, 100);
  argMaxY(dst, vertical, 100);
  imshow("Original", src);
  end = omp_get_wtime();

  cout << vertical[0] << " " << vertical[1] << endl;
  cout << horizontal[0] << " " << horizontal[1] << endl;

  drawLines(dst, temp, vertical, horizontal);
  namedWindow("final");
  imshow("final", temp);

  namedWindow("Original");
  cout<<"time is: "<<(end-start)<< " seconds" <<endl;
  waitKey();
  return 0;
}
