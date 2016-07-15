#include <boost/algorithm/string.hpp>

#include "Solver.h"

Solver::Solver(const FiniteVolumeGrid2D &grid, const Input &input)
    :
      grid_(grid),
      ib_(input, *this)
{
    std::string timeDependentOpt = input.caseInput().get<std::string>("Solver.timeDependent");
    boost::to_lower(timeDependentOpt);

    timeDependent_ = timeDependentOpt == "on" ? ON : OFF;
}

std::string Solver::info() const
{
    return "SOLVER INFO\n"
               "Time dependent: " + std::string((timeDependent_ == ON) ? "On" : "Off") + "\n";
}

ScalarFiniteVolumeField& Solver::addScalarField(const Input& input, const std::string& name)
{
    typedef std::pair< std::string, ScalarFiniteVolumeField> Key;
    return (scalarFields_.insert(Key(name, ScalarFiniteVolumeField(input, grid_, name))).first)->second;
}

ScalarFiniteVolumeField& Solver::addScalarField(const std::string& name)
{
    typedef std::pair< std::string, ScalarFiniteVolumeField> Key;
    return (scalarFields_.insert(Key(name, ScalarFiniteVolumeField(grid_, name))).first)->second;
}

VectorFiniteVolumeField& Solver::addVectorField(const Input& input, const std::string& name)
{
    typedef std::pair< std::string, VectorFiniteVolumeField> Key;
    return (vectorFields_.insert(Key(name, VectorFiniteVolumeField(input, grid_, name))).first)->second;
}

VectorFiniteVolumeField& Solver::addVectorField(const std::string& name)
{
    typedef std::pair< std::string, VectorFiniteVolumeField> Key;
    return (vectorFields_.insert(Key(name, VectorFiniteVolumeField(grid_, name))).first)->second;
}

std::vector<Polygon>& Solver::addGeometries(const std::string &name)
{
    return (geometries_.insert(std::make_pair(name, std::vector<Polygon>(grid_.cells().size()))).first)->second;
}

void Solver::setInitialConditions(const Input& input)
{
    using namespace std;
    using namespace boost::property_tree;

    for(const auto& child: input.initialConditionInput().get_child("InitialConditions"))
    {
        auto scalarFieldIt = scalarFields().find(child.first);

        if(scalarFieldIt != scalarFields().end())
        {
            ScalarFiniteVolumeField &field = scalarFieldIt->second;

            for(const auto& ic: child.second)
            {
                const auto &icTree = ic.second;
                std::string type = icTree.get<string>("type");

                if(type == "circle")
                {
                    Circle circle = Circle(Vector2D(icTree.get<string>("center")), icTree.get<Scalar>("radius"));
                    setCircle(circle, icTree.get<Scalar>("value"), field);
                }
                else if(type == "box")
                {
                    Point2D center = Point2D(icTree.get<string>("center"));
                    Scalar w = icTree.get<Scalar>("width")/2;
                    Scalar h = icTree.get<Scalar>("height")/2;

                    std::vector<Point2D> vertices = {
                        Point2D(center.x - w, center.y - h),
                        Point2D(center.x + w, center.y - h),
                        Point2D(center.x + w, center.y + h),
                        Point2D(center.x - w, center.y + h)
                    };

                    setBox(Polygon(vertices), icTree.get<Scalar>("value"), field);
                }
                else if(type == "uniform")
                    field.fillInterior(icTree.get<Scalar>("value"));
                else if(type == "rotating")
                {
                    setRotating(icTree.get<std::string>("function"),
                                icTree.get<Scalar>("amplitude"),
                                Vector2D(icTree.get<std::string>("center")),
                                field);
                }

                printf("Set initial condition \"%s\" of type %s on field \"%s\".\n", ic.first.c_str(), type.c_str(), field.name.c_str());
            }

            continue;
        }

        auto vectorFieldIt = vectorFields().find(child.first);

        if(vectorFieldIt != vectorFields().end())
        {
            VectorFiniteVolumeField &field = vectorFieldIt->second;

            for(const auto& ic: child.second)
            {
                const auto &icTree = ic.second;
                std::string type = icTree.get<string>("type");

                if(type == "circle")
                {
                    Circle circle = Circle(Vector2D(icTree.get<string>("center")), icTree.get<Scalar>("radius"));
                    setCircle(circle, Vector2D(icTree.get<string>("value")), field);
                }
                else if(type == "square")
                {
                    Point2D center = Point2D(icTree.get<string>("center"));
                    Scalar w = icTree.get<Scalar>("width")/2;
                    Scalar h = icTree.get<Scalar>("height")/2;

                    std::vector<Point2D> vertices = {
                        Point2D(center.x - w, center.y - h),
                        Point2D(center.x + w, center.y - h),
                        Point2D(center.x + w, center.y + h),
                        Point2D(center.x - w, center.y + h)
                    };

                    setBox(Polygon(vertices), Vector2D(icTree.get<string>("value")), field);
                }
                else if(type == "uniform")
                    field.fillInterior(Vector2D(icTree.get<string>("value")));
                else if(type == "rotating")
                {
                    setRotating(icTree.get<std::string>("xFunction"),
                                icTree.get<std::string>("yFunction"),
                                Vector2D(icTree.get<std::string>("amplitude")),
                                Vector2D(icTree.get<std::string>("center")),
                                field);
                }

                printf("Set initial condition \"%s\" of type %s on field \"%s\".\n", ic.first.c_str(), type.c_str(), field.name.c_str());
            }
        }
    }
}

//- Protected methods

void Solver::setCircle(const Circle &circle, Scalar innerValue, ScalarFiniteVolumeField &field)
{
    for(const Cell& cell: field.grid.cells())
    {
        if (circle.isInside(cell.centroid()))
            field[cell.id()] = innerValue;
    }

    interpolateFaces(field);
}

void Solver::setCircle(const Circle &circle, const Vector2D &innerValue, VectorFiniteVolumeField &field)
{
    for(const Cell& cell: field.grid.cells())
    {
        if (circle.isInside(cell.centroid()))
            field[cell.id()] = innerValue;
    }

    interpolateFaces(field);
}

void Solver::setBox(const Polygon& box, Scalar innerValue, ScalarFiniteVolumeField& field)
{
    for(const Cell& cell: field.grid.cells())
    {
        if (box.isInside(cell.centroid()))
            field[cell.id()] = innerValue;
    }

    interpolateFaces(field);
}

void Solver::setBox(const Polygon& box, const Vector2D& innerValue, VectorFiniteVolumeField& field)
{
    for(const Cell& cell: field.grid.cells())
    {
        if (box.isInside(cell.centroid()))
            field[cell.id()] = innerValue;
    }

    interpolateFaces(field);
}

void Solver::setRotating(const std::string &function, Scalar amplitude, const Vector2D &center, ScalarFiniteVolumeField &field)
{
    std::function<Scalar(Scalar)> func;

    if(function == "sin")
        func = sin;
    else if(function == "cos")
        func = cos;
    else
        throw Exception("Input", "setRotating", "invalid rotation function.");

    for(const Cell& cell: field.grid.cells())
    {
        Vector2D rVec = cell.centroid() - center;
        Scalar theta = atan2(rVec.y, rVec.x);

        field[cell.id()] = amplitude*func(theta);
    }

    for(const Face& face: field.grid.interiorFaces())
    {
        Vector2D rVec = face.centroid() - center;
        Scalar theta = atan2(rVec.y, rVec.x);

        field.faces()[face.id()] = amplitude*func(theta);
    }
}

void Solver::setRotating(const std::string &xFunction, const std::string &yFunction, const Vector2D &amplitude, const Vector2D &center, VectorFiniteVolumeField &field)
{
    std::function<Scalar(Scalar)> xFunc, yFunc;

    if(xFunction == "sin")
        xFunc = sin;
    else if(xFunction == "cos")
        xFunc = cos;
    else
        throw Exception("Input", "setRotating", "invalid x rotation function.");

    if(yFunction == "sin")
        yFunc = sin;
    else if(yFunction == "cos")
        yFunc = cos;
    else
        throw Exception("Input", "setRotating", "invalid y rotation function.");

    for(const Cell& cell: field.grid.cells())
    {
        Vector2D rVec = cell.centroid() - center;
        Scalar theta = atan2(rVec.y, rVec.x);

        field[cell.id()].x = amplitude.x*xFunc(theta);
        field[cell.id()].y = amplitude.y*yFunc(theta);
    }

    for(const Face& face: field.grid.interiorFaces())
    {
        Vector2D rVec = face.centroid() - center;
        Scalar theta = atan2(rVec.y, rVec.x);

        field.faces()[face.id()].x = amplitude.x*xFunc(theta);
        field.faces()[face.id()].y = amplitude.y*yFunc(theta);
    }
}
