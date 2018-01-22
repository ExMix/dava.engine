/******************************************************************************
BigWorld Technology 
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "planeeq.hpp"

/**
*	Intersection plane with segment.
*	Returns true if has intersection, otherwise returns false.
*/
bool PlaneEq::intersectSegment(DAVA::Vector3& point, const DAVA::Vector3& a, const DAVA::Vector3& b) const
{
    const DAVA::Vector3 ab = b - a;

    // check division by zero, if result is -INF +INF or NAN, then all is well.
    float normalDotDir = normal_.DotProduct(ab);

    if (almostZero(normalDotDir))
        return false;

    float t = (d_ - normal_.DotProduct(a)) / normalDotDir;

    if (t >= 0.0f && t <= 1.0f)
    {
        point = a + t * ab;
        return true;
    }

    return false;
}

/**
 *	Plane intersection function. The line is defined in the 'this' plane.
 */
LineEq PlaneEq::intersect(const PlaneEq& slice) const
{
    using namespace DAVA;

    const PlaneEq& basis = *this;

    // find the direction of our line in 3D
    Vector3 dir3D;
    dir3D.CrossProduct(basis.normal(), slice.normal());
    //	Vector3 dir3D = basis.normal();
    //	dir3D.CrossProduct( slice.normal() );

    if (almostZero(dir3D.SquareLength(), 0.0001f))
        return LineEq(Vector2(0.f, 0.f), 0.f);

    // find the normal to the line (on the first plane) in 3D
    Vector3 normal3D;
    normal3D.CrossProduct(normal_, dir3D);
    //	Vector3 normal3D = normal_;
    //	normal3D.CrossProduct( dir3D );

    // find the origin of the basis plane's coordinate system in 3D
    Vector3 origin3D = this->param(Vector2(0, 0));

    // find the intersection in the direction of normal3D through
    //  origin3D with the slice plane.
    Vector3 intersectPoint3D = slice.intersectRay(origin3D, normal3D);

    // now turn intersectionPoint and dir3D into the basis of the basis plane
    Vector2 intersectPoint2D = this->project(intersectPoint3D);

    // set the normal from the normal in 3D
    Vector2 lineNormal = this->project(origin3D + normal3D);
    lineNormal.Normalize();

    // calc d_ to work for the point that we know is on the line
    float lineD = lineNormal.DotProduct(intersectPoint2D);

    Vector3 ir = this->param(intersectPoint2D);

    if ((ir - intersectPoint3D).Length() > 1.f)
    {
        Logger::Debug("Intersecting (%f,%f,%f) > %f with (%f,%f,%f) > %f\n",
                      basis.normal().x, basis.normal().y, basis.normal().z, basis.d(),
                      slice.normal().x, slice.normal().y, slice.normal().z, slice.d());

        Logger::Debug("  3D intersect point (%f,%f,%f)\n",
                      intersectPoint3D.x, intersectPoint3D.y, intersectPoint3D.z);
        Logger::Debug("  intersectPoint2D (%f,%f)\n", intersectPoint2D.x, intersectPoint2D.y);
        Logger::Debug("  Reconstituted intersect point (%f,%f,%f)\n", ir.x, ir.y, ir.z);
        Vector3 xdir, ydir;
        this->basis(xdir, ydir);
        Logger::Debug("  xdir (%f,%f,%f)\n", xdir.x, xdir.y, xdir.z);
        Logger::Debug("  ydir (%f,%f,%f)\n", ydir.x, ydir.y, ydir.z);
        Logger::Debug("  xdir dot ydir: %f\n", xdir.DotProduct(ydir));
    }

    return LineEq(lineNormal, lineD);
}

const int biMoreL[3] = { 1, 2, 0 };
const int biLessL[3] = { 2, 0, 1 };

/**
 *	This function returns the x and y directions for the parameterised
 *	basis of the given plane.
 */
void PlaneEq::basis(DAVA::Vector3& xdir, DAVA::Vector3& ydir) const
{
    // find a suitable x basis vector  (find biggest dimension of normal.
    //  set other dimension A to zero. set other dimension B to one.
    //  then calc what biggest dimension should be to put us on the plane.
    //  we choose the biggest because we have to divide by it)
    float fabses[3] =
    { float(fabs(normal_.x)), float(fabs(normal_.y)), float(fabs(normal_.z)) };
    int bi = fabses[0] > fabses[1] ?
    (fabses[0] > fabses[2] ? 0 : 2) :
    (fabses[1] > fabses[2] ? 1 : 2);

    int biMore = biMoreL[bi];
    int biLess = biLessL[bi];

    xdir.data[biLess] = 0;
    xdir.data[biMore] = 1;
    xdir.data[bi] = -normal_.data[biMore] / normal_.data[bi];

    // find a suitable y basis vector  (cross product of existing ones)
    ydir.CrossProduct(normal_, xdir);
    //	ydir = normal_;
    //	ydir.CrossProduct(xdir);

    xdir.Normalize();
    ydir.Normalize();
}

/**
 *	This function finds the point defined by the 'point' param on the
 *	given plane. It should be a method of PlaneEq.
 */
DAVA::Vector3 PlaneEq::param(const DAVA::Vector2& param) const
{
    using namespace DAVA;

    // get the basis to use
    Vector3 xdir, ydir;
    this->basis(xdir, ydir);

    float dOverNormLen = d_ / normal_.SquareLength();

    // and get the vector. the 'origin' of the plane is at
    //  normal * d / normal.SquareLength
    return dOverNormLen * normal_ + param.x * xdir + param.y * ydir;
}

/**
 *	This function finds the param for the given point in the plane's basis
 */
DAVA::Vector2 PlaneEq::project(const DAVA::Vector3& point) const
{
    using namespace DAVA;

    // get the basis to use
    Vector3 xdir, ydir;
    this->basis(xdir, ydir);

    // take off the origin of the plane
    Vector3 offset = point - (d_ / normal_.SquareLength()) * normal_;

    // and project it onto our (unit length) basis vectors
    return Vector2(
    xdir.DotProduct(offset),
    ydir.DotProduct(offset));
}

// planeeq.cpp
