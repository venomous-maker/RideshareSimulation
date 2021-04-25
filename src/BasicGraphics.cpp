#include <chrono>
#include <iostream>
#include <thread>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include "BasicGraphics.h"

BasicGraphics::BasicGraphics(float min_lat, float min_lon, float max_lat, float max_lon) {
    min_lat_ = min_lat;
    min_lon_ = min_lon;
    max_lat_ = max_lat;
    max_lon_ = max_lon;
}

void BasicGraphics::Simulate() {
    this->LoadBackgroundImg();
    while (true) {
        // sleep at every iteration to reduce CPU usage
        // TODO: Shorten below delay once ready for active updating
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));

        // update graphics
        this->DrawSimulation();
    }
}

void BasicGraphics::LoadBackgroundImg() {
    // create window
    windowName_ = "Rideshare Simulation";
    cv::namedWindow(windowName_, cv::WINDOW_NORMAL);

    // load image and create copy to be used for semi-transparent overlay
    cv::Mat background = cv::imread(bgFilename_);
    images_.push_back(background);         // first element is the original background
    images_.push_back(background.clone()); // second element will be the transparent overlay
    images_.push_back(background.clone()); // third element will be the result image for display
}

void BasicGraphics::DrawSimulation() {
    // reset images
    images_.at(1) = images_.at(0).clone();
    images_.at(2) = images_.at(0).clone();

    DrawIntersections();
    DrawPassengers();

    // display background and overlay image
    cv::imshow(windowName_, images_.at(2));
    cv::waitKey(33);
}

void BasicGraphics::DrawIntersections() {
    // Get image rows and columns to help with coordinate adjustments for image
    float img_rows = images_.at(0).rows;
    float img_cols = images_.at(0).cols;

    // create overlay from intersections
    for (auto intersect : intersections_) {
        std::vector<float> position = intersect.GetPosition();

        // Adjust the position based on lat & lon in image
        position[0] = (position[0] - min_lon_) / (max_lon_ - min_lon_);
        position[1] = (max_lat_ - position[1]) / (max_lat_ - min_lat_);

        // set color according to traffic light and draw the intersection as a circle
        //std::cout << "Position at: " << (int)(position[0] * imgCols) << "," << (int)(position[1] * imgRows) << std::endl;
        cv::circle(images_.at(1), cv::Point2d((int)(position[0] * img_cols), (int)(position[1] * img_rows)), 25, cv::Scalar(0, 255, 0), -1);
    }

    float opacity = 0.85;
    cv::addWeighted(images_.at(1), opacity, images_.at(0), 1.0 - opacity, 0, images_.at(2));
    std::cout << "Image size is: " << img_rows << " rows, and " << img_cols << " columns." << std::endl;
}

void BasicGraphics::DrawPassengers() {
    // Get image rows and columns to help with coordinate adjustments for image
    float img_rows = images_.at(0).rows;
    float img_cols = images_.at(0).cols;

    // create overlay from passengers
    for (auto passenger : passenger_queue_.NewPassengers()) {
        std::vector<float> start_position = {passenger.StartX(), passenger.StartY()};
        std::vector<float> dest_position = {passenger.DestX(), passenger.DestY()};

        // Adjust the position based on lat & lon in image
        // TODO: Change below to just need one position?
        start_position[0] = (start_position[0] - min_lon_) / (max_lon_ - min_lon_);
        start_position[1] = (max_lat_ - start_position[1]) / (max_lat_ - min_lat_);
        dest_position[0] = (dest_position[0] - min_lon_) / (max_lon_ - min_lon_);
        dest_position[1] = (max_lat_ - dest_position[1]) / (max_lat_ - min_lat_);

        // TODO: Only show the current position of passenger, perhaps with destination as different shape?
        //std::cout << "Position at: " << (int)(start_position[0] * imgCols) << "," << (int)(start_position[1] * imgRows) << std::endl;
        cv::Scalar color = cv::Scalar(passenger.Blue(), passenger.Green(), passenger.Red());
        cv::circle(images_.at(1), cv::Point2d((int)(start_position[0] * img_cols), (int)(start_position[1] * img_rows)), 25, color, -1);
        cv::circle(images_.at(1), cv::Point2d((int)(dest_position[0] * img_cols), (int)(dest_position[1] * img_rows)), 25, color, -1);
    }

    float opacity = 0.85;
    cv::addWeighted(images_.at(1), opacity, images_.at(0), 1.0 - opacity, 0, images_.at(2));
}