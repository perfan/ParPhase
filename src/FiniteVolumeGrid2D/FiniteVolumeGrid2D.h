#ifndef FINITE_VOLUME_GRID_2D_H
#define FINITE_VOLUME_GRID_2D_H

#include <vector>
#include <map>

#include "Node.h"
#include "Cell.h"
#include "Face.h"
#include "Patch.h"
#include "BoundingBox.h"
#include "CellZone.h"
#include "Search.h"
#include "Communicator.h"

class FiniteVolumeGrid2D
{
public:

    FiniteVolumeGrid2D(Size nNodes = 0, Size nCells = 0, Size nFaces = 0);

    //- Initialization
    void init(const std::vector<Point2D>& nodes, const std::vector<Label>& cellInds, const std::vector<Label>& cells);
    void reset();

    //- Size info
    Size nNodes() const { return nodes_.size(); }
    Size nCells() const { return cells_.size(); }
    Size nFaces() const { return faces_.size(); }
    Size nActiveCells() const { return activeCells().size(); }
    std::string gridInfo() const;

    //- Create grid entities
    Label createCell(const std::vector<Label> &nodeIds);
    Label addNode(const Point2D& point);

    //- Node related methods
    const std::vector<Node>& nodes() const { return nodes_; }
    std::vector<Point2D> coords() const;
    std::vector<Scalar> xCoords() const;
    std::vector<Scalar> yCoords() const;
    void assignNodeIds();

    //- Cell related methods
    const std::vector<Cell>& cells() const { return cells_; }
    std::vector<int> elementList() const;

    //- Cell groups and zones
    CellGroup &cellGroup(const std::string& name) const { return cellGroups_.find(name)->second; }
    CellZone &cellZone(const std::string& name) const { return cellZones_.find(name)->second; }
    CellZone &addCellZone(const std::string& name, const std::vector<Label>& ids);

    const CellGroup& activeCells() const { return cellGroup("active"); }
    const CellGroup& globallyActiveCells() const { return cellGroup("globallyActive"); }
    const CellZone& inactiveCells() const { return cellZone("inactive"); }
    const CellZone& fluidCells() const { return cellZone("fluid"); }

    const Cell& findContainingCell(const Point2D& point, const Cell &guess) const;

    CellZone& moveCellsToFluidCellGroup(const std::vector<size_t>& ids);
    CellZone& moveAllCellsToFluidCellGroup();
    CellZone& moveCellsToInactiveCellGroup(const std::vector<size_t>& ids);
    CellZone& moveCellsToCellGroup(const std::string& name, const std::vector<size_t>& ids);
    void removeFromActiveCellGroup(const std::vector<Label>& ids);

    const std::vector< Ref<const Cell> > getCells(const std::vector<Label>& ids) const;

    void assignCellIds();

    //- Face related methods
    const std::vector<Face>& faces() const { return faces_; }

    const std::vector< Ref<const Face> >& interiorFaces() const { return interiorFaces_; }
    const std::vector< Ref<const Face> >& boundaryFaces() const { return boundaryFaces_; }

    bool faceExists(Label n1, Label n2) const;
    Label findFace(Label n1, Label n2) const;

    void assignFaceIds();

    //- Patch related methods
    const std::map<std::string, Patch>& patches() const { return patches_; }

    //- Entity searches
    const Node& findNearestNode(const Point2D& pt) const;
    std::vector<std::vector<Ref<const Cell> > > constructSmoothingKernels(Scalar width) const;

    //- Parallel/paritioning
    std::pair<std::vector<int>, std::vector<int>> nodeElementConnectivity() const;
    void partition(const Communicator& comm);

    template<typename T>
    void sendMessages(const Communicator& comm, std::vector<T>& data) const;

    void addNeighbouringProc(int procNo,
                             const std::vector<Label>& sendOrder,
                             const std::vector<Label>& recvOrder);

    std::pair<int, int> globalCellRange() const;

    //- Misc
    const BoundingBox& boundingBox() const { return bBox_; }

protected:

    void initNodes();
    void initCells();
    void initConnectivity();
    void initGlobalIds(const Communicator& comm);

    void constructActiveCellGroup();
    void computeBoundingBox();
    void applyPatch(const std::string& patchName, const std::vector<Label> &faces);
    void applyPatchByNodes(const std::string& patchName, const std::vector<Label>& nodes);

    //- Node related data
    std::vector<Node> nodes_;

    //- Cell related data
    std::vector<Cell> cells_;

    mutable std::map<std::string, CellGroup> cellGroups_;
    mutable std::map<std::string, CellZone> cellZones_;
    std::vector<int> neighbouringProcs_;
    std::vector<std::vector<Label>> procSendOrder_;
    std::vector<std::vector<Label>> procRecvOrder_;

    //- Face related data
    std::vector<Face> faces_;
    std::map<std::pair<Label, Label>, Label> faceDirectory_; // A directory that can find a face given the two node ids

    //- Interior adn boundary face data structures
    std::vector< Ref<const Face> > interiorFaces_; // All interior faces neighboured by two active cells
    std::vector< Ref<const Face> > boundaryFaces_; // All boundary faces neighboured by one active cell

    //- Patches
    std::map<std::string, Patch> patches_;

    BoundingBox bBox_;
    Polygon domain_;

    //- For node searches
    Search<Node> nodeSearch_;
};

#include "FiniteVolumeGrid2D.tpp"

#endif
