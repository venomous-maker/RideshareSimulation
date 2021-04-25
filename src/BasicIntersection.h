#ifndef BASICINTERSECTION_H
#define BASICINTERSECTION_H

#include <vector>

class BasicIntersection {
  public:
    // constructor / destructor

    // getters / setters
    void SetPosition(float x, float y) { x_ = x; y_ = y; }
    std::vector<float> GetPosition() { return {x_, y_}; }

  private:
    // member variables
    float x_, y_;
};

#endif