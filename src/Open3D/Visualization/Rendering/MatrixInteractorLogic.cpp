// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "MatrixInteractorLogic.h"

namespace open3d {
namespace visualization {

MatrixInteractorLogic::~MatrixInteractorLogic() {}

void MatrixInteractorLogic::SetViewSize(int width, int height) {
    viewWidth_ = width;
    viewHeight_ = height;
}

const geometry::AxisAlignedBoundingBox& MatrixInteractorLogic::GetBoundingBox()
        const {
    return modelBounds_;
}

void MatrixInteractorLogic::SetBoundingBox(
        const geometry::AxisAlignedBoundingBox& bounds) {
    modelSize_ = (bounds.GetMaxBound() - bounds.GetMinBound()).norm();
    modelBounds_ = bounds;
}

void MatrixInteractorLogic::SetMouseDownInfo(
        const Camera::Transform& matrix,
        const Eigen::Vector3f& centerOfRotation) {
    matrix_ = matrix;
    centerOfRotation_ = centerOfRotation;

    matrixAtMouseDown_ = matrix;
    centerOfRotationAtMouseDown_ = centerOfRotation;
}

void MatrixInteractorLogic::SetMatrix(const Camera::Transform& matrix) {
    matrix_ = matrix;
}

const Camera::Transform& MatrixInteractorLogic::GetMatrix() const {
    return matrix_;
}

void MatrixInteractorLogic::Rotate(int dx, int dy) {
    auto matrix = matrixAtMouseDown_;  // copy
    Eigen::AngleAxisf rotMatrix(0, Eigen::Vector3f(1, 0, 0));

    // We want to rotate as if we were rotating an imaginary trackball
    // centered at the point of rotation. To do this we need an axis
    // of rotation and an angle about the axis. To find the axis, we
    // imagine that the viewing plane has been translated into the screen
    // so that it intersects the center of rotation. The axis we want
    // to rotate around is perpendicular to the vector defined by (dx, dy)
    // (assuming +x is right and +y is up). (Imagine the situation if the
    // mouse movement is (100, 0) or (0, 100).) Now it is easy to find
    // the perpendicular in 2D. Conveniently, (axis.x, axis.y, 0) is the
    // correct axis in camera-local coordinates. We can multiply by the
    // camera's rotation matrix to get the correct world vector.
    dy = -dy;  // up is negative, but the calculations are easiest to
               // imagine up is positive.
    Eigen::Vector3f axis(-dy, dx, 0);  // rotate by 90 deg in 2D
    axis = axis.normalized();
    float theta = CalcRotateRadians(dx, dy);

    axis = matrix.rotation() * axis;  // convert axis to world coords
    rotMatrix = rotMatrix * Eigen::AngleAxisf(-theta, axis);

    auto pos = matrix * Eigen::Vector3f(0, 0, 0);
    Eigen::Vector3f toCOR = centerOfRotation_ - pos;
    auto dist = toCOR.norm();
    // If the center of rotation is behind the camera we need to flip
    // the sign of 'dist'. We can just dotprod with the forward vector
    // of the camera. Forward is [0, 0, -1] for an identity matrix,
    // so forward is simply rotation * [0, 0, -1].
    Eigen::Vector3f forward =
            matrix.rotation() * Eigen::Vector3f{0.0f, 0.0f, -1.0f};
    if (toCOR.dot(forward) < 0) {
        dist = -dist;
    }
    visualization::Camera::Transform m;
    m.fromPositionOrientationScale(centerOfRotation_,
                                   rotMatrix * matrix.rotation(),
                                   Eigen::Vector3f(1, 1, 1));
    m.translate(Eigen::Vector3f(0, 0, dist));

    matrix_ = m;
}

void MatrixInteractorLogic::RotateWorld(int dx,
                                        int dy,
                                        const Eigen::Vector3f& xAxis,
                                        const Eigen::Vector3f& yAxis) {
    auto matrix = matrixAtMouseDown_;  // copy

    dy = -dy;  // up is negative, but the calculations are easiest to
               // imagine up is positive.
    Eigen::Vector3f axis = dx * xAxis + dy * yAxis;
    axis = axis.normalized();
    float theta = CalcRotateRadians(dx, dy);

    axis = matrix.rotation() * axis;  // convert axis to world coords
    auto rotMatrix = visualization::Camera::Transform::Identity() *
                     Eigen::AngleAxisf(-theta, axis);

    auto pos = matrix * Eigen::Vector3f(0, 0, 0);
    auto dist = (centerOfRotation_ - pos).norm();
    visualization::Camera::Transform m;
    m.fromPositionOrientationScale(centerOfRotation_,
                                   rotMatrix * matrix.rotation(),
                                   Eigen::Vector3f(1, 1, 1));
    m.translate(Eigen::Vector3f(0, 0, dist));

    matrix_ = m;
}

double MatrixInteractorLogic::CalcRotateRadians(int dx, int dy) {
    Eigen::Vector3f moved(dx, dy, 0);
    return 0.5 * M_PI * moved.norm() / (0.5f * float(viewHeight_));
}

void MatrixInteractorLogic::RotateZ(int dx, int dy) {
    // RotateZ rotates around the axis normal to the screen. Since we
    // will be rotating using camera coordinates, we want to rotate
    // about (0, 0, 1).
    Eigen::Vector3f axis(0, 0, 1);
    auto rad = CalcRotateZRadians(dx, dy);
    auto matrix = matrixAtMouseDown_;  // copy
    matrix.rotate(Eigen::AngleAxisf(rad, axis));
    matrix_ = matrix;
}

void MatrixInteractorLogic::RotateZWorld(int dx,
                                         int dy,
                                         const Eigen::Vector3f& forward) {
    auto rad = CalcRotateZRadians(dx, dy);
    Eigen::AngleAxisf rotMatrix(rad, forward);

    Camera::Transform matrix = matrixAtMouseDown_;  // copy
    matrix.translate(centerOfRotation_);
    matrix *= rotMatrix;
    matrix.translate(-centerOfRotation_);
    matrix_ = matrix;
}

double MatrixInteractorLogic::CalcRotateZRadians(int dx, int dy) {
    // Moving half the height should rotate 360 deg (= 2 * PI).
    // This makes it easy to rotate enough without rotating too much.
    return 4.0 * M_PI * dy / viewHeight_;
}

void MatrixInteractorLogic::Dolly(int dy, DragType dragType) {
    float dist = CalcDollyDist(dy, dragType);
    if (dragType == DragType::MOUSE) {
        Dolly(dist, matrixAtMouseDown_);  // copies the matrix
    } else {
        Dolly(dist, matrix_);
    }
}

// Note: we pass `matrix` by value because we want to copy it,
//       as translate() will be modifying it.
void MatrixInteractorLogic::Dolly(float zDist, Camera::Transform matrix) {
    // Dolly is just moving the camera forward. Filament uses right as +x,
    // up as +y, and forward as -z (standard OpenGL coordinates). So to
    // move forward all we need to do is translate the camera matrix by
    // dist * (0, 0, -1). Note that translating by camera_->GetForwardVector
    // would be incorrect, since GetForwardVector() returns the forward
    // vector in world space, but the translation happens in camera space.)
    // Since we want trackpad down (negative) to go forward ("pulling" the
    // model toward the viewer) we need to negate dy.
    auto forward = Eigen::Vector3f(0, 0, -zDist);  // zDist * (0, 0, -1)
    matrix.translate(forward);
    matrix_ = matrix;
}

float MatrixInteractorLogic::CalcDollyDist(int dy, DragType dragType) {
    float dist = 0.0f;  // initialize to make GCC happy
    switch (dragType) {
        case DragType::MOUSE:
            // Zoom out is "push away" or up, is a negative value for
            // mousing
            dist = float(dy) * 0.0025f * modelSize_;
            break;
        case DragType::TWO_FINGER:
            // Zoom out is "push away" or up, is a positive value for
            // two-finger scrolling, so we need to invert dy.
            dist = float(-dy) * 0.01f * modelSize_;
            break;
        case DragType::WHEEL:  // actual mouse wheel, same as two-fingers
            dist = float(-dy) * 0.1f * modelSize_;
            break;
    }
    return dist;
}

}  // namespace visualization
}  // namespace open3d