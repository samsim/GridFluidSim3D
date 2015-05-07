#include "fluidsimulation.h"


FluidSimulation::FluidSimulation() :
                                _bodyForce(glm::vec3(0.0, 0.0, 0.0)),
                                _MACVelocity(_i_voxels, _j_voxels, _k_voxels, _dx),
                                _materialGrid(Array3d<int>(_i_voxels, _j_voxels, _k_voxels, M_AIR)),
                                _pressureGrid(Array3d<double>(_i_voxels, _j_voxels, _k_voxels, 0.0)),
                                _layerGrid(Array3d<int>(_i_voxels, _j_voxels, _k_voxels, -1))
{
}

FluidSimulation::FluidSimulation(int x_voxels, int y_voxels, int z_voxels, double cell_size) : 
                                 _i_voxels(x_voxels), _j_voxels(y_voxels), _k_voxels(z_voxels), _dx(cell_size), 
                                 _bodyForce(glm::vec3(0.0, 0.0, 0.0)),
                                 _MACVelocity(x_voxels, y_voxels, z_voxels, cell_size),
                                 _materialGrid(Array3d<int>(x_voxels, y_voxels, z_voxels, M_AIR)),
                                 _pressureGrid(Array3d<double>(x_voxels, y_voxels, z_voxels, 0.0)),
                                 _layerGrid(Array3d<int>(x_voxels, y_voxels, z_voxels, -1)),
                                 _implicitFluidField(x_voxels*cell_size, y_voxels*cell_size, z_voxels*cell_size)
{
}

FluidSimulation::~FluidSimulation() {

}

void FluidSimulation::run() {
    if (!_isSimulationInitialized) {
        _initializeSimulation();
    }
    _isSimulationRunning = true;
}

void FluidSimulation::pause() {
    if (!_isSimulationInitialized) {
        return;
    }

    _isSimulationRunning = !_isSimulationRunning;
}

void FluidSimulation::addBodyForce(glm::vec3 f) {
    _bodyForce += f;
}

void FluidSimulation::setBodyForce(glm::vec3 f) {
    _bodyForce = f;
}

void FluidSimulation::addImplicitFluidPoint(glm::vec3 p, double r) {
    _implicitFluidField.addPoint(p, r);
}

void FluidSimulation::addFluidCuboid(glm::vec3 p1, glm::vec3 p2) {
    glm::vec3 minp = glm::vec3(fmin(p1.x, p2.x),
                               fmin(p1.y, p2.y), 
                               fmin(p1.z, p2.z));
    double width = fabs(p2.x - p1.x);
    double height = fabs(p2.y - p1.y);
    double depth = fabs(p2.z - p1.z);

    addFluidCuboid(minp, width, height, depth);
}

void FluidSimulation::addFluidCuboid(glm::vec3 p, double w, double h, double d) {
    _implicitFluidField.addCuboid(p, w, h, d);
}

std::vector<ImplicitPointData> FluidSimulation::getImplicitFluidPoints() {
    return _implicitFluidField.getImplicitPointData();
}

std::vector<glm::vec3> FluidSimulation::getMarkerParticles(int skip) {
    std::vector<glm::vec3> particles;

    for (int i = 0; i < (int)_markerParticles.size(); i += skip) {
        particles.push_back(_markerParticles[i].position);
    }

    return particles;
}

std::vector<glm::vec3> FluidSimulation::getMarkerParticles() {
    return getMarkerParticles(1);
}

void FluidSimulation::_initializeSolidCells() {
    // fill borders with solid cells
    for (int j = 0; j < _j_voxels; j++) {
        for (int i = 0; i < _i_voxels; i++) {
            _materialGrid.set(i, j, 0, M_SOLID);
            _materialGrid.set(i, j, _k_voxels-1, M_SOLID);
        }
    }

    for (int k = 0; k < _k_voxels; k++) {
        for (int i = 0; i < _i_voxels; i++) {
            _materialGrid.set(i, 0, k, M_SOLID);
            _materialGrid.set(i, _j_voxels-1, k, M_SOLID);
        }
    }

    for (int k = 0; k < _k_voxels; k++) {
        for (int j = 0; j < _j_voxels; j++) {
            _materialGrid.set(0, j, k, M_SOLID);
            _materialGrid.set(_i_voxels-1, j, k, M_SOLID);
        }
    }

    /*
    double ci = floor(_i_voxels / 2);
    double cj = floor(_j_voxels / 2);
    double ck = floor(_k_voxels / 2);
    double height = 5;
    double width = 9;
    for (int k = 0; k < _k_voxels; k++) {
        for (int j = cj; j < cj + height; j++) {
            for (int i = 0; i < _i_voxels; i++) {
                if (!(i >= ci - width && i <= ci + width && k >= ck - width && k <= ck + width)) {
                    _materialGrid.set(i, j, k, M_SOLID);
                }
            }
        }
    }
    */
}

void FluidSimulation::_addMarkerParticlesToCell(int i, int j, int k) {
    double q = 0.25*_dx;
    double cx, cy, cz;
    gridIndexToCellCenter(i, j, k, &cx, &cy, &cz);

    glm::vec3 points[] = {
        glm::vec3(cx - q, cy - q, cz - q),
        glm::vec3(cx + q, cy - q, cz - q),
        glm::vec3(cx + q, cy - q, cz + q),
        glm::vec3(cx - q, cy - q, cz + q),
        glm::vec3(cx - q, cy + q, cz - q),
        glm::vec3(cx + q, cy + q, cz - q),
        glm::vec3(cx + q, cy + q, cz + q),
        glm::vec3(cx - q, cy + q, cz + q)
    };

    double eps = 10e-6;
    double jitter = 0.25*_dx - eps;

    for (int idx = 0; idx < 8; idx++) {
        glm::vec3 jit = glm::vec3(_randomFloat(-jitter, jitter),
                                  _randomFloat(-jitter, jitter),
                                  _randomFloat(-jitter, jitter));

        glm::vec3 p = points[idx] + jit;
        _markerParticles.push_back(MarkerParticle(p, i, j, k));
    }
}

void FluidSimulation::_initializeFluidMaterial() {
    _isFluidInSimulation = _implicitFluidField.getNumPoints() > 0 ||
                           _implicitFluidField.getNumCuboids() > 0;

    if (!_isFluidInSimulation) {
        return;
    }

    for (int k = 0; k < _materialGrid.depth; k++) {
        for (int j = 0; j < _materialGrid.height; j++) {
            for (int i = 0; i < _materialGrid.width; i++) {
                double x, y, z;
                gridIndexToCellCenter(i, j, k, &x, &y, &z);

                if (_implicitFluidField.isInside(x, y, z) && _isCellAir(i, j, k)) {
                    _materialGrid.set(i, j, k, M_FLUID);
                    _addMarkerParticlesToCell(i, j, k);
                }
            }
        }
    }
}

void FluidSimulation::_initializeSimulation() {
    _initializeSolidCells();
    _initializeFluidMaterial();
    _isSimulationInitialized = true;
}

glm::vec3 FluidSimulation::_RK2(glm::vec3 p0, glm::vec3 v0, double dt) {
    glm::vec3 k1 = v0;
    glm::vec3 k2 = _MACVelocity.evaluateVelocityAtPosition(p0 + (float)(0.5*dt)*k1);
    glm::vec3 p1 = p0 + (float)dt*k2;

    return p1;
}

glm::vec3 FluidSimulation::_RK3(glm::vec3 p0, glm::vec3 v0, double dt) {
    glm::vec3 k1 = v0;
    glm::vec3 k2 = _MACVelocity.evaluateVelocityAtPosition(p0 + (float)(0.5*dt)*k1);
    glm::vec3 k3 = _MACVelocity.evaluateVelocityAtPosition(p0 + (float)(0.75*dt)*k2);
    glm::vec3 p1 = p0 + (float)(dt/9.0f)*(2.0f*k1 + 3.0f*k2 + 4.0f*k3);

    return p1;
}

glm::vec3 FluidSimulation::_RK4(glm::vec3 p0, glm::vec3 v0, double dt) {
    glm::vec3 k1 = v0;
    glm::vec3 k2 = _MACVelocity.evaluateVelocityAtPosition(p0 + (float)(0.5*dt)*k1);
    glm::vec3 k3 = _MACVelocity.evaluateVelocityAtPosition(p0 + (float)(0.5*dt)*k2);
    glm::vec3 k4 = _MACVelocity.evaluateVelocityAtPosition(p0 + (float)dt*k3);
    glm::vec3 p1 = p0 + (float)(dt/6.0f)*(k1 + 2.0f*k2 + 2.0f*k3 + k4);

    return p1;
}

double FluidSimulation::_calculateNextTimeStep() {
    double maxu = _MACVelocity.evaluateMaximumVelocityMagnitude();
    double timeStep = _CFLConditionNumber*_dx / maxu;

    timeStep = fmaxf(_minTimeStep, timeStep);
    timeStep = fminf(_maxTimeStep, timeStep);

    return timeStep;
}

bool FluidSimulation::_isPointOnCellFace(glm::vec3 p, CellFace f) {
    double eps = 10e-6;

    if (fabs(fabs(f.normal.x) - 1.0) < eps) {
        return fabs(p.x - f.minx) < eps &&
               p.y >= f.miny && p.y < f.maxy && p.z >= f.minz && p.z < f.maxz;
    }
    else if (fabs(fabs(f.normal.y) - 1.0) < eps) {
        return fabs(p.y - f.miny) < eps &&
               p.x >= f.minx && p.x < f.maxx && p.z >= f.minz && p.z < f.maxz;
    }
    else if (fabs(fabs(f.normal.z) - 1.0) < eps) {
        return fabs(p.z - f.minz) < eps &&
               p.x >= f.minx && p.x < f.maxx && p.y >= f.miny && p.y < f.maxy;
    }

    return false;
}

FluidSimulation::CellFace FluidSimulation::_getCellFace(int i, int j, int k, 
                                                        glm::vec3 normal) {
    assert(_isCellIndexInRange(i, j, k));
    
    double eps = 10e-4;
    glm::vec3 trans;
    if (fabs(fabs(normal.x) - 1.0) < eps) {
        trans = (float)(0.5 * _dx) * glm::vec3(0.0, 1.0, 1.0);
    }
    else if (fabs(fabs(normal.y) - 1.0) < eps) {
        trans = (float)(0.5 * _dx) * glm::vec3(1.0, 0.0, 1.0);
    }
    else if (fabs(fabs(normal.z) - 1.0) < eps) {
        trans = (float)(0.5 * _dx) * glm::vec3(1.0, 1.0, 0.0);
    }

    glm::vec3 c = gridIndexToCellCenter(i, j, k);
    glm::vec3 minp = c + (float)(0.5*_dx)*normal - trans;
    glm::vec3 maxp = c + (float)(0.5*_dx)*normal + trans;

    return CellFace(normal, minp, maxp);
}

void FluidSimulation::_getCellFaces(int i, int j, int k, CellFace faces[6]) {
    faces[0] = _getCellFace(i, j, k, glm::vec3(-1.0,  0.0,  0.0));
    faces[1] = _getCellFace(i, j, k, glm::vec3( 1.0,  0.0,  0.0));
    faces[2] = _getCellFace(i, j, k, glm::vec3( 0.0, -1.0,  0.0));
    faces[3] = _getCellFace(i, j, k, glm::vec3( 0.0,  1.0,  0.0));
    faces[4] = _getCellFace(i, j, k, glm::vec3( 0.0,  0.0, -1.0));
    faces[5] = _getCellFace(i, j, k, glm::vec3( 0.0,  0.0,  1.0));
}

std::vector<FluidSimulation::CellFace> FluidSimulation::
             _getNeighbourSolidCellFaces(int i, int j, int k) {

    assert(_isCellIndexInRange(i, j, k));

    std::vector<CellFace> faces;
    glm::vec3 normals[6] = { glm::vec3(-1.0, 0.0, 0.0), glm::vec3(1.0, 0.0, 0.0),
                             glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 1.0, 0.0), 
                             glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, 0.0, 1.0) };
    GridIndex nc[26];
    _getNeighbourGridIndices26(i, j, k, nc);
    for (int idx = 0; idx < 26; idx++) {
        GridIndex c = nc[idx];
        if (_isCellIndexInRange(c.i, c.j, c.k) && _isCellSolid(c.i, c.j, c.k)) {
            for (int normidx = 0; normidx < 6; normidx++) {
                faces.push_back(_getCellFace(c.i, c.j, c.k, normals[normidx]));
            }
        }
    }

    return faces;
}

bool FluidSimulation::_getVectorFaceIntersection(glm::vec3 p0, glm::vec3 vnorm, CellFace f, 
                                                 glm::vec3 *intersect) {

    // perpendicular or p0 is on the plane
    double eps = 10e-30;
    double dot = glm::dot(vnorm, f.normal);
    if (fabs(dot) < eps) {
        return false;
    }

    glm::vec3 planep = glm::vec3(f.minx, f.miny, f.minz);
    double d = glm::dot((planep - p0), f.normal) / dot;

    *intersect = p0 + (float)d*vnorm;

    if (_isPointOnCellFace(*intersect, f)) {
        return true;
    }

    return false;
}

// Check if p lies on a cell face which borders a fluid cell and a solid cell. 
// if so, *f will store the face with normal pointing away from solid
bool FluidSimulation::_isPointOnSolidFluidBoundary(glm::vec3 p, CellFace *f) {
    int i, j, k;
    positionToGridIndex(p, &i, &j, &k);

    int U = 0; int V = 1; int W = 2;
    int side;
    int dir;
    CellFace faces[6];
    _getCellFaces(i, j, k, faces);
    for (int idx = 0; idx < 6; idx++) {
        if (_isPointOnCellFace(p, faces[idx])) {
            glm::vec3 n = faces[idx].normal;

            int fi, fj, fk;
            if      (n.x == -1.0) { fi = i;     fj = j;     fk = k;     side = U; dir = -1; }
            else if (n.x == 1.0)  { fi = i + 1; fj = j;     fk = k;     side = U; dir =  1; }
            else if (n.y == -1.0) { fi = i;     fj = j;     fk = k;     side = V; dir = -1; }
            else if (n.y == 1.0)  { fi = i;     fj = j + 1; fk = k;     side = V; dir =  1; }
            else if (n.z == -1.0) { fi = i;     fj = j;     fk = k;     side = W; dir = -1; }
            else if (n.z == 1.0)  { fi = i;     fj = j;     fk = k + 1; side = W; dir =  1; }

            bool isCellSolid = _isCellSolid(i, j, k);
            if      (side == U && _isFaceBorderingMaterialU(fi, fj, fk, M_SOLID)) {
                if (dir == -1) {
                    *f = isCellSolid ? _getCellFace(i, j, k, glm::vec3(-1.0, 0.0, 0.0)) :
                                       _getCellFace(i - 1, j, k, glm::vec3(1.0, 0.0, 0.0));
                }
                else {
                    *f = isCellSolid ? _getCellFace(i, j, k, glm::vec3(1.0, 0.0, 0.0)) :
                                       _getCellFace(i + 1, j, k, glm::vec3(-1.0, 0.0, 0.0));
                }
                return true;
            }
            else if (side == V && _isFaceBorderingMaterialV(fi, fj, fk, M_SOLID)) {
                if (dir == -1) {
                    *f = isCellSolid ? _getCellFace(i, j, k, glm::vec3(0.0, -1.0, 0.0)) :
                                       _getCellFace(i, j - 1, k, glm::vec3(0.0, 1.0, 0.0));
                }
                else {
                    *f = isCellSolid ? _getCellFace(i, j, k, glm::vec3(0.0, 1.0, 0.0)) :
                                       _getCellFace(i, j + 1, k, glm::vec3(0.0, -1.0, 0.0));
                }
                return true;
            }
            else if (side == W && _isFaceBorderingMaterialW(fi, fj, fk, M_SOLID)) {
                if (dir == -1) {
                    *f = isCellSolid ? _getCellFace(i, j, k, glm::vec3(0.0, 0.0, -1.0)) :
                                       _getCellFace(i, j, k - 1, glm::vec3(0.0, 0.0, 1.0));
                }
                else {
                    *f = isCellSolid ? _getCellFace(i, j, k, glm::vec3(0.0, 0.0, 1.0)) :
                                       _getCellFace(i, j, k + 1, glm::vec3(0.0, 0.0, -1.0));
                }
                return true;
            }
        }
    }

    return false;
}

std::vector<FluidSimulation::CellFace> FluidSimulation::
        _getSolidCellFaceCollisionCandidates(int i, int j, int k, glm::vec3 dir) {
    std::vector<CellFace> faces;

    std::vector<CellFace> allfaces = _getNeighbourSolidCellFaces(i, j, k);
    for (int idx = 0; idx < allfaces.size(); idx++) {
        // must be obtuse angle for a collision
        if (glm::dot(allfaces[idx].normal, dir) < 0) {
            faces.push_back(allfaces[idx]);
        }
    }

    return faces;
}

bool FluidSimulation::_findFaceCollision(glm::vec3 p0, glm::vec3 p1, CellFace *face, glm::vec3 *intersection) {
    int i, j, k;
    positionToGridIndex(p0, &i, &j, &k);
    glm::vec3 vnorm = glm::normalize(p1 - p0);
    std::vector<CellFace> faces = _getSolidCellFaceCollisionCandidates(i, j, k, vnorm);

    glm::vec3 closestIntersection;
    CellFace closestFace;
    double mindistsq = std::numeric_limits<double>::infinity();
    bool isCollisionFound = false;
    for (int idx = 0; idx < faces.size(); idx++) {
        CellFace f = faces[idx];

        glm::vec3 intersect;
        bool isIntersecting = _getVectorFaceIntersection(p0, vnorm, f, &intersect);

        if (!isIntersecting) {
            continue;
        }

        glm::vec3 trans = intersect - p0;
        double distsq = glm::dot(trans, trans);
        if (distsq < mindistsq) {
            mindistsq = distsq;
            closestIntersection = intersect;
            closestFace = f;
            isCollisionFound = true;
        }
    }

    if (isCollisionFound) {
        *face = closestFace;
        *intersection = closestIntersection;
    }

    return isCollisionFound;
}

glm::vec3 FluidSimulation::_calculateSolidCellCollision(glm::vec3 p0, 
                                                        glm::vec3 p1, 
                                                        glm::vec3 *normal) {

    // p0 might lie right on a boundary face. In this case, the cell index of 
    // p0 may be calculated as a solid cell
    CellFace boundaryFace;
    bool isOnBoundary = _isPointOnSolidFluidBoundary(p0, &boundaryFace);
    if (isOnBoundary) {
        *normal = boundaryFace.normal;
        return p0;
    }

    int fi, fj, fk, si, sj, sk;
    positionToGridIndex(p0, &fi, &fj, &fk);
    positionToGridIndex(p1, &si, &sj, &sk);
    assert(!_isCellSolid(fi, fj, fk));
    assert(_isCellSolid(si, sj, sk));

    // p0 and p1 may not be located in neighbouring cells. Find
    // the neighbouring cell and a point in the cell just before collision 
    // with solid wall. Keep stepping back from p1 until solid collision neighbours
    // are found.
    glm::vec3 vnorm = glm::normalize(p1 - p0);
    int numSteps = 1;
    while (!_isCellNeighbours(fi, fj, fk, si, sj, sk)) {
        p0 = p1 - (float)(_dx - 10e-6)*vnorm;
        int newi, newj, newk;
        positionToGridIndex(p0, &newi, &newj, &newk);

        if (_isCellSolid(newi, newj, newk)) {
            p1 = p0;
            si = newi; sj = newj; sk = newk;
        }
        else {
            fi = newi, fj = newj, fk = newk;
        }

        numSteps++;
        assert(numSteps < 100);
        assert(!(fi == si && fj == sj && fk == sk));
    }

    CellFace collisionFace;
    glm::vec3 collisionPoint;
    bool isCollisionFound = _findFaceCollision(p0, p1, &collisionFace, &collisionPoint);

    if (isCollisionFound) {
        *normal = collisionFace.normal;

        // testing: jog point back from face and make sure it is not in solid cell
        int i, j, k;
        glm::vec3 p2 = collisionPoint + float(0.001*_dx)*(*normal);
        positionToGridIndex(p2, &i, &j, &k);
        
        if (_isCellSolid(i, j, k)) {
            std::cout << "Error: Solid collision " <<
                p0.x << " " << p0.y << " " << p0.z << " " << p1.x << " " << p1.y << " " << p1.z << std::endl;
            std::cout << vnorm.x << " " << vnorm.y << " " << vnorm.z << std::endl;
            std::cout << fi << " " << fj << " " << fk << " " << si << " " << sj << " " << sk << std::endl;
        }
        assert(!_isCellSolid(i, j, k));

        return collisionPoint;
    }
    else {
        std::cout << "Error: collision not found " << 
            p0.x << " " << p0.y << " " << p0.z << " " << p1.x << " " << p1.y << " " << p1.z << std::endl;
        std::cout << vnorm.x << " " << vnorm.y << " " << vnorm.z << std::endl;
        std::cout << fi << " " << fj << " " << fk << " " << si << " " << sj << " " << sk << std::endl;
        *normal = glm::vec3(0.0, 0.0, 0.0);

        return p0;
    }
}

bool FluidSimulation::_integrateVelocity(glm::vec3 p0, glm::vec3 v0, double dt, glm::vec3 *p1) {
    *p1 = _RK4(p0, v0, dt);

    int ni, nj, nk;
    positionToGridIndex(*p1, &ni, &nj, &nk);

    if (_isCellSolid(ni, nj, nk)) {
        glm::vec3 collisionNormal;
        *p1 = _calculateSolidCellCollision(p0, *p1, &collisionNormal);
        
        // jog p1 back a bit from cell face
        *p1 = *p1 + (float)(0.01*_dx)*collisionNormal;

        positionToGridIndex(*p1, &ni, &nj, &nk);
        if (_isCellSolid(ni, nj, nk)) {
            *p1 = p0;
        }

        return false;
    }
    else {
        return true;
    }
}

void FluidSimulation::_backwardsAdvectVelocity(glm::vec3 p0, glm::vec3 v0, double dt, 
                                               glm::vec3 *p1, glm::vec3 *v1) {
    double timeleft = dt;
    while (timeleft > 0.0) {
        double timestep = _dx / glm::length(v0);
        timestep = fminf(timeleft, timestep);
        bool isOutsideBoundary = _integrateVelocity(p0, v0, -timestep, p1);
        *v1 = _MACVelocity.evaluateVelocityAtPosition(*p1);
        if (!isOutsideBoundary) {
            break;
        }

        p0 = *p1; v0 = *v1;
        timeleft -= timestep;
    }
}

void FluidSimulation::_advectVelocityFieldU(double dt) {
    glm::vec3 p0, p1, v0, v1;
    for (int k = 0; k < _k_voxels; k++) {
        for (int j = 0; j < _j_voxels; j++) {
            for (int i = 0; i < _i_voxels + 1; i++) {
                if (_isFaceBorderingMaterialU(i, j, k, M_FLUID)) {
                    p0 = _MACVelocity.velocityIndexToPositionU(i, j, k);
                    v0 = _MACVelocity.evaluateVelocityAtFaceCenterU(i, j, k);
                    _backwardsAdvectVelocity(p0, v0, dt, &p0, &v1);
                    _MACVelocity.setTempU(i, j, k, v1.x);
                }
            }
        }
    }
}

void FluidSimulation::_advectVelocityFieldV(double dt) {
    glm::vec3 p0, p1, v0, v1;
    for (int k = 0; k < _k_voxels; k++) {
        for (int j = 0; j < _j_voxels + 1; j++) {
            for (int i = 0; i < _i_voxels; i++) {
                if (_isFaceBorderingMaterialV(i, j, k, M_FLUID)) {
                    p0 = _MACVelocity.velocityIndexToPositionV(i, j, k);
                    v0 = _MACVelocity.evaluateVelocityAtFaceCenterV(i, j, k);
                    _backwardsAdvectVelocity(p0, v0, dt, &p0, &v1);
                    _MACVelocity.setTempV(i, j, k, v1.y);
                }
            }
        }
    }
}

void FluidSimulation::_advectVelocityFieldW(double dt) {
    glm::vec3 p0, p1, v0, v1;
    for (int k = 0; k < _k_voxels + 1; k++) {
        for (int j = 0; j < _j_voxels; j++) {
            for (int i = 0; i < _i_voxels; i++) {
                if (_isFaceBorderingMaterialW(i, j, k, M_FLUID)) {
                    p0 = _MACVelocity.velocityIndexToPositionW(i, j, k);
                    v0 = _MACVelocity.evaluateVelocityAtFaceCenterW(i, j, k);
                    _backwardsAdvectVelocity(p0, v0, dt, &p0, &v1);
                    _MACVelocity.setTempW(i, j, k, v1.z);
                }
            }
        }
    }
}

void FluidSimulation::_advectVelocityField(double dt) {
    _MACVelocity.resetTemporaryVelocityField();
    
    std::thread t1 = std::thread(&FluidSimulation::_advectVelocityFieldU, this, dt);
    std::thread t2 = std::thread(&FluidSimulation::_advectVelocityFieldV, this, dt);
    std::thread t3 = std::thread(&FluidSimulation::_advectVelocityFieldW, this, dt);

    t1.join();
    t2.join();
    t3.join();

    _MACVelocity.commitTemporaryVelocityFieldValues();
}

glm::vec3 FluidSimulation::gridIndexToPosition(GridIndex g) {
    assert(_isCellIndexInRange(g.i, g.j, g.k));

    double x, y, z;
    gridIndexToPosition(g, &x, &y, &z);

    return glm::vec3(x, y, z);
}

void FluidSimulation::gridIndexToPosition(GridIndex g, double *x, double *y, double *z) {
    assert(_isCellIndexInRange(g.i, g.j, g.k));

    *x = (double)g.i*_dx;
    *y = (double)g.j*_dx;
    *z = (double)g.k*_dx;
}

void FluidSimulation::gridIndexToPosition(int i, int j, int k, double *x, double *y, double *z) {
    assert(_isCellIndexInRange(i, j, k));

    *x = (double)i*_dx;
    *y = (double)j*_dx;
    *z = (double)k*_dx;
}

void FluidSimulation::gridIndexToCellCenter(int i, int j, int k, double *x, double *y, double *z) {
    assert(_isCellIndexInRange(i, j, k));

    *x = (double)i*_dx + 0.5*_dx;
    *y = (double)j*_dx + 0.5*_dx;
    *z = (double)k*_dx + 0.5*_dx;
}

glm::vec3 FluidSimulation::gridIndexToCellCenter(GridIndex g) {
    return gridIndexToCellCenter(g.i, g.j, g.k);
}

glm::vec3 FluidSimulation::gridIndexToCellCenter(int i, int j, int k) {
    assert(_isCellIndexInRange(i, j, k));

    return glm::vec3((double)i*_dx + 0.5*_dx,
                     (double)j*_dx + 0.5*_dx,
                     (double)k*_dx + 0.5*_dx);
}

void FluidSimulation::positionToGridIndex(glm::vec3 p, int *i, int *j, int *k) {
    double invdx = 1.0 / _dx;
    *i = (int)floor(p.x*invdx);
    *j = (int)floor(p.y*invdx);
    *k = (int)floor(p.z*invdx);
}

void FluidSimulation::positionToGridIndex(double x, double y, double z, int *i, int *j, int *k) {
    double invdx = 1.0 / _dx;
    *i = (int)floor(x*invdx);
    *j = (int)floor(y*invdx);
    *k = (int)floor(z*invdx);
}

void FluidSimulation::_updateFluidCells() {
    for (int k = 0; k < _materialGrid.depth; k++) {
        for (int j = 0; j < _materialGrid.height; j++) {
            for (int i = 0; i < _materialGrid.width; i++) {
                if (_isCellFluid(i, j, k)) {
                    _materialGrid.set(i, j, k, M_AIR);
                }
            }
        }
    }

    MarkerParticle p;
    for (int i = 0; i < (int)_markerParticles.size(); i++) {
        p = _markerParticles[i];
        assert(!_isCellSolid(p.i, p.j, p.k));
        _materialGrid.set(p.i, p.j, p.k, M_FLUID);
    }

    _fluidCellIndices.clear();
    for (int k = 0; k < _materialGrid.depth; k++) {
        for (int j = 0; j < _materialGrid.height; j++) {
            for (int i = 0; i < _materialGrid.width; i++) {
                if (_isCellFluid(i, j, k)) {
                    _fluidCellIndices.push_back(GridIndex(i, j, k));
                }
            }
        }
    }
}

void FluidSimulation::_getNeighbourGridIndices6(int i, int j, int k, GridIndex n[6]) {
    n[0] = GridIndex(i-1, j, k);
    n[1] = GridIndex(i+1, j, k);
    n[2] = GridIndex(i, j-1, k);
    n[3] = GridIndex(i, j+1, k);
    n[4] = GridIndex(i, j, k-1);
    n[5] = GridIndex(i, j, k+1);
}

void FluidSimulation::_getNeighbourGridIndices26(int i, int j, int k, GridIndex n[26]) {
    int idx = 0;
    for (int nk = k-1; nk <= k+1; nk++) {
        for (int nj = j-1; nj <= j+1; nj++) {
            for (int ni = i-1; ni <= i+1; ni++) {
                if (!(ni == i && nj == j && nk == k)) {
                    n[idx] = GridIndex(ni, nj, nk);
                    idx++;
                }
            }
        }
    }
}

void FluidSimulation::_updateExtrapolationLayer(int layerIndex) {
    GridIndex neighbours[6];
    GridIndex n;

    for (int k = 0; k < _layerGrid.depth; k++) {
        for (int j = 0; j < _layerGrid.height; j++) {
            for (int i = 0; i < _layerGrid.width; i++) {
                if (_layerGrid(i, j, k) == layerIndex - 1 && !_isCellSolid(i, j, k)) {
                    _getNeighbourGridIndices6(i, j, k, neighbours);
                    for (int idx = 0; idx < 6; idx++) {
                        n = neighbours[idx];

                        if (_isCellIndexInRange(n.i, n.j, n.k) && _layerGrid(n.i, n.j, n.k) == -1 &&
                            !_isCellSolid(n.i, n.j, n.k)) {
                            _layerGrid.set(n.i, n.j, n.k, layerIndex);
                        }
                    }
                }
            }
        }
    }
}

int FluidSimulation::_updateExtrapolationLayers() {
    _layerGrid.fill(-1);

    GridIndex idx;
    for (int i = 0; i < (int)_fluidCellIndices.size(); i++) {
        idx = _fluidCellIndices[i];
        _layerGrid.set(idx.i, idx.j, idx.k, 0);
    }

    // add 2 extra layers to account for extra values needed during cubic 
    // interpolation calculations
    int numLayers = ceil(_CFLConditionNumber) + 2;
    for (int layer = 1; layer <= numLayers; layer++) {
        _updateExtrapolationLayer(layer);
    }

    return numLayers;
}

double FluidSimulation::_getExtrapolatedVelocityForFaceU(int i, int j, int k, int layerIdx) {
    GridIndex n[6];
    _getNeighbourGridIndices6(i, j, k, n);

    GridIndex c;
    double sum = 0.0;
    double weightsum = 0.0;

    for (int idx = 0; idx < 6; idx++) {
        c = n[idx];
        if (_MACVelocity.isIndexInRangeU(c.i, c.j, c.k) && 
            _isFaceBorderingLayerIndexU(c.i, c.j, c.k, layerIdx - 1)) {
                sum += _MACVelocity.U(c.i, c.j, c.k);
                weightsum++;
        }
    }

    if (sum == 0.0) {
        return 0.0;
    }

    return sum / weightsum;
}

double FluidSimulation::_getExtrapolatedVelocityForFaceV(int i, int j, int k, int layerIdx) {
    GridIndex n[6];
    _getNeighbourGridIndices6(i, j, k, n);

    GridIndex c;
    double sum = 0.0;
    double weightsum = 0.0;

    for (int idx = 0; idx < 6; idx++) {
        c = n[idx];
        if (_MACVelocity.isIndexInRangeV(c.i, c.j, c.k) &&
            _isFaceBorderingLayerIndexV(c.i, c.j, c.k, layerIdx - 1)) {
            sum += _MACVelocity.V(c.i, c.j, c.k);
            weightsum++;
        }
    }

    if (sum == 0.0) {
        return 0.0;
    }

    return sum / weightsum;
}

double FluidSimulation::_getExtrapolatedVelocityForFaceW(int i, int j, int k, int layerIdx) {
    GridIndex n[6];
    _getNeighbourGridIndices6(i, j, k, n);

    GridIndex c;
    double sum = 0.0;
    double weightsum = 0.0;

    for (int idx = 0; idx < 6; idx++) {
        c = n[idx];
        if (_MACVelocity.isIndexInRangeW(c.i, c.j, c.k) &&
            _isFaceBorderingLayerIndexW(c.i, c.j, c.k, layerIdx - 1)) {
            sum += _MACVelocity.W(c.i, c.j, c.k);
            weightsum++;
        }
    }

    if (sum == 0.0) {
        return 0.0;
    }

    return sum / weightsum;
}

void FluidSimulation::_extrapolateVelocitiesForLayerIndex(int idx) {
    _MACVelocity.resetTemporaryVelocityField();

    for (int k = 0; k < _k_voxels; k++) {
        for (int j = 0; j < _j_voxels; j++) {
            for (int i = 0; i < _i_voxels + 1; i++) {
                if (_isFaceBorderingLayerIndexU(i, j, k, idx) &&
                    !_isFaceBorderingLayerIndexU(i, j, k, idx-1) &&
                    !_isFaceBorderingMaterialU(i, j, k, M_SOLID)) {
                    double v = _getExtrapolatedVelocityForFaceU(i, j, k, idx);
                    _MACVelocity.setTempU(i, j, k, v);
                }
            }
        }
    }

    for (int k = 0; k < _k_voxels; k++) {
        for (int j = 0; j < _j_voxels + 1; j++) {
            for (int i = 0; i < _i_voxels; i++) {
                if (_isFaceBorderingLayerIndexV(i, j, k, idx) &&
                    !_isFaceBorderingLayerIndexV(i, j, k, idx - 1) &&
                    !_isFaceBorderingMaterialV(i, j, k, M_SOLID)) {
                    double v = _getExtrapolatedVelocityForFaceV(i, j, k, idx);
                    _MACVelocity.setTempV(i, j, k, v);
                }
            }
        }
    }

    for (int k = 0; k < _k_voxels + 1; k++) {
        for (int j = 0; j < _j_voxels; j++) {
            for (int i = 0; i < _i_voxels; i++) {
                if (_isFaceBorderingLayerIndexW(i, j, k, idx) &&
                    !_isFaceBorderingLayerIndexW(i, j, k, idx - 1) &&
                    !_isFaceBorderingMaterialW(i, j, k, M_SOLID)) {
                    double v = _getExtrapolatedVelocityForFaceW(i, j, k, idx);
                    _MACVelocity.setTempW(i, j, k, v);
                }
            }
        }
    }

    _MACVelocity.commitTemporaryVelocityFieldValues();
}

void FluidSimulation::_resetExtrapolatedFluidVelocities() {
    for (int k = 0; k < _k_voxels; k++) {
        for (int j = 0; j < _j_voxels; j++) {
            for (int i = 0; i < _i_voxels + 1; i++) {
                if (!_isFaceBorderingMaterialU(i, j, k, M_FLUID)) {
                    _MACVelocity.setU(i, j, k, 0.0);
                }
            }
        }
    }

    for (int k = 0; k < _k_voxels; k++) {
        for (int j = 0; j < _j_voxels + 1; j++) {
            for (int i = 0; i < _i_voxels; i++) {
                if (!_isFaceBorderingMaterialV(i, j, k, M_FLUID)) {
                    _MACVelocity.setV(i, j, k, 0.0);
                }
            }
        }
    }

    for (int k = 0; k < _k_voxels + 1; k++) {
        for (int j = 0; j < _j_voxels; j++) {
            for (int i = 0; i < _i_voxels; i++) {
                if (!_isFaceBorderingMaterialW(i, j, k, M_FLUID)) {
                    _MACVelocity.setW(i, j, k, 0.0);
                }
            }
        }
    }
}

void FluidSimulation::_extrapolateFluidVelocities() {
    _resetExtrapolatedFluidVelocities();
    int numLayers = _updateExtrapolationLayers();

    for (int i = 1; i <= numLayers; i++) {
        _extrapolateVelocitiesForLayerIndex(i);
    }
}

void FluidSimulation::_applyBodyForcesToVelocityField(double dt) {
    if (fabs(_bodyForce.x) > 0.0) {
        for (int k = 0; k < _k_voxels; k++) {
            for (int j = 0; j < _j_voxels; j++) {
                for (int i = 0; i < _i_voxels + 1; i++) {
                    if (_isFaceBorderingMaterialU(i, j, k, M_FLUID) ||
                            _isFaceVelocityExtrapolatedU(i, j, k)) {
                        _MACVelocity.addU(i, j, k, _bodyForce.x * dt);
                    }
                }
            }
        }
    }

    if (fabs(_bodyForce.y) > 0.0) {
        for (int k = 0; k < _k_voxels; k++) {
            for (int j = 0; j < _j_voxels + 1; j++) {
                for (int i = 0; i < _i_voxels; i++) {
                    if (_isFaceBorderingMaterialV(i, j, k, M_FLUID) ||
                            _isFaceVelocityExtrapolatedV(i, j, k)) {
                        _MACVelocity.addV(i, j, k, _bodyForce.y * dt);
                    }
                }
            }
        }
    }

    if (fabs(_bodyForce.z) > 0.0) {
        for (int k = 0; k < _k_voxels + 1; k++) {
            for (int j = 0; j < _j_voxels; j++) {
                for (int i = 0; i < _i_voxels; i++) {
                    if (_isFaceBorderingMaterialW(i, j, k, M_FLUID) ||
                            _isFaceVelocityExtrapolatedW(i, j, k)) {
                        _MACVelocity.addW(i, j, k, _bodyForce.z * dt);
                    }
                }
            }
        }
    }
}

double FluidSimulation::_calculateNegativeDivergenceVector(VectorCoefficients &b) {
    double scale = 1.0 / _dx;

    for (int idx = 0; idx < (int)_fluidCellIndices.size(); idx++) {
        int i = _fluidCellIndices[idx].i;
        int j = _fluidCellIndices[idx].j;
        int k = _fluidCellIndices[idx].k;

        double value = -scale * (_MACVelocity.U(i + 1, j, k) - _MACVelocity.U(i, j, k) +
                                 _MACVelocity.V(i, j + 1, k) - _MACVelocity.V(i, j, k) +
                                 _MACVelocity.W(i, j, k + 1) - _MACVelocity.W(i, j, k));
        b.vector.set(i, j, k, value);
    }

    // solid cells are stationary right now
    double usolid = 0.0;
    double vsolid = 0.0;
    double wsolid = 0.0;
    double maxDivergence = 0.0;
    for (int idx = 0; idx < (int)_fluidCellIndices.size(); idx++) {
        int i = _fluidCellIndices[idx].i;
        int j = _fluidCellIndices[idx].j;
        int k = _fluidCellIndices[idx].k;

        if (_isCellSolid(i-1, j, k)) {
            double value = b.vector(i, j, k) - scale*(_MACVelocity.U(i, j, k) - usolid);
            b.vector.set(i, j, k, value);
        }
        if (_isCellSolid(i+1, j, k)) {
            double value = b.vector(i, j, k) + scale*(_MACVelocity.U(i+1, j, k) - usolid);
            b.vector.set(i, j, k, value);
        }

        if (_isCellSolid(i, j-1, k)) {
            double value = b.vector(i, j, k) - scale*(_MACVelocity.V(i, j, k) - usolid);
            b.vector.set(i, j, k, value);
        }
        if (_isCellSolid(i, j+1, k)) {
            double value = b.vector(i, j, k) + scale*(_MACVelocity.V(i, j+1, k) - usolid);
            b.vector.set(i, j, k, value);
        }

        if (_isCellSolid(i, j, k-1)) {
            double value = b.vector(i, j, k) - scale*(_MACVelocity.W(i, j, k) - usolid);
            b.vector.set(i, j, k, value);
        }
        if (_isCellSolid(i, j, k+1)) {
            double value = b.vector(i, j, k) + scale*(_MACVelocity.W(i, j, k+1) - usolid);
            b.vector.set(i, j, k, value);
        }

        maxDivergence = fmax(maxDivergence, fabs(b.vector(i, j, k)));
    }

    return maxDivergence;
}

void FluidSimulation::_calculateMatrixCoefficients(MatrixCoefficients &A, double dt) {
    double scale = dt / (_density*_dx*_dx);

    for (int idx = 0; idx < (int)_fluidCellIndices.size(); idx ++) {
        int i = _fluidCellIndices[idx].i;
        int j = _fluidCellIndices[idx].j;
        int k = _fluidCellIndices[idx].k;

        if (_isCellFluid(i + 1, j, k)) {
            A.diag.add(i, j, k, scale);
            A.diag.add(i + 1, j, k, scale);
            A.plusi.set(i, j, k, -scale);
        }
        else if (_isCellAir(i + 1, j, k)) {
            A.diag.add(i, j, k, scale);
        }

        if (_isCellFluid(i, j + 1, k)) {
            A.diag.add(i, j, k, scale);
            A.diag.add(i, j + 1, k, scale);
            A.plusj.set(i, j, k, -scale);
        }
        else if (_isCellAir(i, j + 1, k)) {
            A.diag.add(i, j, k, scale);
        }

        if (_isCellFluid(i, j, k + 1)) {
            A.diag.add(i, j, k, scale);
            A.diag.add(i, j, k + 1, scale);
            A.plusk.set(i, j, k, -scale);
        }
        else if (_isCellAir(i, j, k + 1)) {
            A.diag.add(i, j, k, scale);
        }

    }

}

void FluidSimulation::_calculatePreconditionerVector(VectorCoefficients &p, 
                                                     MatrixCoefficients &A) {
    double tau = 0.97;      // Tuning constant
    double sigma = 0.25;    // safety constant
    for (int idx = 0; idx < (int)_fluidCellIndices.size(); idx++) {
        int i = _fluidCellIndices[idx].i;
        int j = _fluidCellIndices[idx].j;
        int k = _fluidCellIndices[idx].k;

        double v1 = A.plusi(i - 1, j, k)*p.vector(i - 1, j, k);
        double v2 = A.plusj(i, j - 1, k)*p.vector(i, j - 1, k);
        double v3 = A.plusk(i, j, k - 1)*p.vector(i, j, k - 1);
        double v4 = p.vector(i - 1, j, k); v4 = v4*v4;
        double v5 = p.vector(i, j - 1, k); v5 = v5*v5;
        double v6 = p.vector(i, j, k - 1); v6 = v6*v6;

        double e = A.diag(i, j, k) - v1*v1 - v2*v2 - v3*v3 - 
            tau*(A.plusi(i - 1, j, k)*(A.plusj(i - 1, j, k) + A.plusk(i - 1, j, k))*v4 +
                 A.plusj(i, j - 1, k)*(A.plusi(i, j - 1, k) + A.plusk(i, j - 1, k))*v5 +
                 A.plusk(i, j, k - 1)*(A.plusi(i, j, k - 1) + A.plusj(i, j, k - 1))*v6);

        if (e < sigma*A.diag(i, j, k)) {
            e = A.diag(i, j, k);
        }

        if (fabs(e) > 10e-9) {
            p.vector.set(i, j, k, 1 / sqrt(e));
        }
    }
}

Eigen::VectorXd FluidSimulation::_VectorCoefficientsToEigenVectorXd(VectorCoefficients &v,
                                                                    std::vector<GridIndex> indices) {
    Eigen::VectorXd ev((int)indices.size());
    for (int idx = 0; idx < (int)indices.size(); idx++) {
        int i = indices[idx].i;
        int j = indices[idx].j;
        int k = indices[idx].k;

        ev(idx) = v.vector(i, j, k);
    }

    return ev;
}

void FluidSimulation::_EigenVectorXdToVectorCoefficients(Eigen::VectorXd v, 
                                                         VectorCoefficients &vc) {
    for (int idx = 0; idx < v.size(); idx++) {
        GridIndex index = _VectorIndexToGridIndex(idx);
        vc.vector.set(index.i, index.j, index.k, v(idx));
    }
}

unsigned long long int FluidSimulation::_calculateGridIndexHash(GridIndex &index) {
    return (unsigned long long)index.i + (unsigned long long)_i_voxels *
          ((unsigned long long)index.j +
           (unsigned long long)_k_voxels * (unsigned long long)index.k);
}

void FluidSimulation::_updateFluidGridIndexToEigenVectorXdIndexHashTable() {
    _GridIndexToEigenVectorXdIndex.clear();

    for (int idx = 0; idx < (int)_fluidCellIndices.size(); idx++) {
        unsigned long long key = _calculateGridIndexHash(_fluidCellIndices[idx]);
        std::pair<unsigned long long, int> keyvalue(key, idx);
        _GridIndexToEigenVectorXdIndex.insert(keyvalue);
    }
}

GridIndex FluidSimulation::_VectorIndexToGridIndex(int i) {
    return _fluidCellIndices[i];
}

int FluidSimulation::_GridIndexToVectorIndex(int i, int j, int k) {
    GridIndex g(i, j, k);
    return _GridIndexToVectorIndex(g);
}

int FluidSimulation::_GridIndexToVectorIndex(GridIndex index) {
    unsigned long long h = _calculateGridIndexHash(index);
    if (_GridIndexToEigenVectorXdIndex.find(h) == _GridIndexToEigenVectorXdIndex.end()) {
        return -1;
    }

    return _GridIndexToEigenVectorXdIndex[h];
}

Eigen::VectorXd FluidSimulation::_applyPreconditioner(Eigen::VectorXd residualVector,
                                                      VectorCoefficients &p,
                                                      MatrixCoefficients &A) {
    VectorCoefficients r(_i_voxels, _j_voxels, _k_voxels);
    _EigenVectorXdToVectorCoefficients(residualVector, r);

    // Solve Aq = r
    VectorCoefficients q(_i_voxels, _j_voxels, _k_voxels);
    for (int idx = 0; idx < (int)_fluidCellIndices.size(); idx++) {
        GridIndex g = _fluidCellIndices[idx];
        int i = g.i;
        int j = g.j;
        int k = g.k;

        double t = r.vector(i, j, k) -
            A.plusi(i - 1, j, k)*p.vector(i - 1, j, k)*q.vector(i - 1, j, k) -
            A.plusj(i, j - 1, k)*p.vector(i, j - 1, k)*q.vector(i, j - 1, k) -
            A.plusk(i, j, k - 1)*p.vector(i, j, k - 1)*q.vector(i, j, k - 1);

        t = t*p.vector(i, j, k);
        q.vector.set(i, j, k, t);
    }

    // Solve transpose(A)*z = q
    VectorCoefficients z(_i_voxels, _j_voxels, _k_voxels);
    for (int idx = (int)_fluidCellIndices.size() - 1; idx >= 0; idx--) {
        GridIndex g = _fluidCellIndices[idx];
        int i = g.i;
        int j = g.j;
        int k = g.k;

        double precon = p.vector(i, j, k);
        double t = q.vector(i, j, k) -
            A.plusi(i, j, k)*precon*z.vector(i + 1, j, k) -
            A.plusj(i, j, k)*precon*z.vector(i, j + 1, k) -
            A.plusk(i, j, k)*precon*z.vector(i, j, k + 1);

        t = t*precon;
        z.vector.set(i, j, k, t);
    }

    return _VectorCoefficientsToEigenVectorXd(z, _fluidCellIndices);
}

int FluidSimulation::_getNumFluidOrAirCellNeighbours(int i, int j, int k) {
    int n = 0;
    if (!_isCellSolid(i-1, j, k)) { n++; }
    if (!_isCellSolid(i+1, j, k)) { n++; }
    if (!_isCellSolid(i, j-1, k)) { n++; }
    if (!_isCellSolid(i, j+1, k)) { n++; }
    if (!_isCellSolid(i, j, k-1)) { n++; }
    if (!_isCellSolid(i, j, k+1)) { n++; }

    return n;
}

Eigen::SparseMatrix<double> FluidSimulation::_MatrixCoefficientsToEigenSparseMatrix(
                                                    MatrixCoefficients &A, double dt) {
    int size = (int)_fluidCellIndices.size();
    double scale = dt / (_density * _dx*_dx);

    std::vector<Eigen::Triplet<double>> matrixValues;
    matrixValues.reserve(size * (6 + 1));   // max number of non-zero entries 
                                            // (6 neighbours + diagonal) for each row
    for (int idx = 0; idx < (int)_fluidCellIndices.size(); idx++) {
        GridIndex g = _fluidCellIndices[idx];
        int i = g.i;
        int j = g.j;
        int k = g.k;

        int row = idx;
        int col = idx;
        double diag = _getNumFluidOrAirCellNeighbours(i, j, k)*scale;
        matrixValues.push_back(Eigen::Triplet<double>(col, row, diag));

        if (_isCellFluid(i-1, j, k)) {
            col = _GridIndexToVectorIndex(i-1, j, k);
            double coef = A.plusi(i-1, j, k);
            matrixValues.push_back(Eigen::Triplet<double>(col, row, coef));
        }
        if (_isCellFluid(i+1, j, k)) {
            col = _GridIndexToVectorIndex(i+1, j, k);
            double coef = A.plusi(i, j, k);
            matrixValues.push_back(Eigen::Triplet<double>(col, row, coef));
        }

        if (_isCellFluid(i, j-1, k)) {
            col = _GridIndexToVectorIndex(i, j-1, k);
            double coef = A.plusj(i, j-1, k);
            matrixValues.push_back(Eigen::Triplet<double>(col, row, coef));
        }
        if (_isCellFluid(i, j+1, k)) {
            col = _GridIndexToVectorIndex(i, j+1, k);
            double coef = A.plusj(i, j, k);
            matrixValues.push_back(Eigen::Triplet<double>(col, row, coef));
        }

        if (_isCellFluid(i, j, k-1)) {
            col = _GridIndexToVectorIndex(i, j, k-1);
            double coef = A.plusk(i, j, k-1);
            matrixValues.push_back(Eigen::Triplet<double>(col, row, coef));
        }
        if (_isCellFluid(i, j, k+1)) {
            col = _GridIndexToVectorIndex(i, j, k+1);
            double coef = A.plusk(i, j, k);
            matrixValues.push_back(Eigen::Triplet<double>(col, row, coef));
        }
    }

    Eigen::SparseMatrix<double> m(size, size);
    m.setFromTriplets(matrixValues.begin(), matrixValues.end());

    return m;
}

// Solve (A*p = b) with Eigen's conjugate gradient method
Eigen::VectorXd FluidSimulation::_solvePressureSystemWithEigen(MatrixCoefficients &A,
                                                               VectorCoefficients &b,
                                                               VectorCoefficients &precon,
                                                               double dt) {
    int size = (int)_fluidCellIndices.size();
    Eigen::VectorXd pressureVector(size); pressureVector.setZero();
    Eigen::VectorXd bVector = _VectorCoefficientsToEigenVectorXd(b, _fluidCellIndices);
    Eigen::VectorXd residualVector(bVector);

    if (fabs(residualVector.maxCoeff()) < _pressureSolveTolerance) {
        std::cout << "\tCG Iterations: " << 0 << std::endl;
        return pressureVector;
    }

    Eigen::SparseMatrix<double> aMatrix = _MatrixCoefficientsToEigenSparseMatrix(A, dt);

    Eigen::ConjugateGradient<Eigen::SparseMatrix<double> > cg;
    cg.setMaxIterations(_maxPressureSolveIterations); 
    cg.setTolerance(_pressureSolveTolerance);

    cg.compute(aMatrix);
    pressureVector = cg.solve(bVector);
    std::cout << "\tCG Iterations:     " << cg.iterations() << std::endl;
    std::cout << "\testimated error: " << cg.error() << std::endl;

    return pressureVector;
}

// Solve (A*p = b) with Modified Incomplete Cholesky Conjugate Gradient menthod
// (MICCG(0))
Eigen::VectorXd FluidSimulation::_solvePressureSystem(MatrixCoefficients &A,
                                                      VectorCoefficients &b,
                                                      VectorCoefficients &precon,
                                                      double dt) {

    int size = (int)_fluidCellIndices.size();
    double tol = _pressureSolveTolerance;

    Eigen::VectorXd pressureVector(size); pressureVector.setZero();
    Eigen::VectorXd bVector = _VectorCoefficientsToEigenVectorXd(b, _fluidCellIndices);
    Eigen::VectorXd residualVector(bVector);

    if (fabs(residualVector.maxCoeff()) < tol) {
        return pressureVector;
    }

    Eigen::SparseMatrix<double> aMatrix = _MatrixCoefficientsToEigenSparseMatrix(A, dt);
    Eigen::VectorXd preconVector = _VectorCoefficientsToEigenVectorXd(precon, _fluidCellIndices);
    Eigen::VectorXd auxillaryVector = _applyPreconditioner(residualVector, precon, A);
    Eigen::VectorXd searchVector(auxillaryVector);

    double alpha = 0.0;
    double beta = 0.0;
    double sigma = auxillaryVector.dot(residualVector);
    double sigmaNew = 0.0;
    int iterationNumber = 0;

    while (iterationNumber < _maxPressureSolveIterations) {
        auxillaryVector = aMatrix*searchVector;
        alpha = sigma / auxillaryVector.dot(searchVector);
        pressureVector += alpha*searchVector;
        residualVector -= alpha*auxillaryVector;

        if (fabs(residualVector.maxCoeff()) < tol) {
            std::cout << "\tCG Iterations: " << iterationNumber << std::endl;
            return pressureVector;
        }

        auxillaryVector = _applyPreconditioner(residualVector, precon, A);
        sigmaNew = auxillaryVector.dot(residualVector);
        beta = sigmaNew / sigma;
        searchVector = auxillaryVector + beta*searchVector;
        sigma = sigmaNew;

        iterationNumber++;

        if (iterationNumber % 10 == 0) {
        std::cout << "\tIteration #: " << iterationNumber << "\tError:  " <<
            fabs(residualVector.maxCoeff()) << std::endl;
        }
    }

    std::cout << "\tIterations limit reached.\t Error: " << 
        fabs(residualVector.maxCoeff()) << std::endl;

    return pressureVector;
}

void FluidSimulation::_updatePressureGrid(double dt) {
    _pressureGrid.fill(0.0);

    VectorCoefficients b(_i_voxels, _j_voxels, _k_voxels);
    double maxDivergence = _calculateNegativeDivergenceVector(b);
    if (maxDivergence < _pressureSolveTolerance) {
        // all pressure values are near 0.0
        return;
    }

    MatrixCoefficients A(_i_voxels, _j_voxels, _k_voxels);
    VectorCoefficients precon(_i_voxels, _j_voxels, _k_voxels);
    _calculateMatrixCoefficients(A, dt);
    _calculatePreconditionerVector(precon, A);

    _updateFluidGridIndexToEigenVectorXdIndexHashTable();
    Eigen::VectorXd pressures = _solvePressureSystem(A, b, precon, dt);

    for (int idx = 0; idx < (int)_fluidCellIndices.size(); idx++) {
        GridIndex index = _VectorIndexToGridIndex(idx);
        _pressureGrid.set(index.i, index.j, index.k, pressures(idx));
    }
}

void FluidSimulation::_advanceRangeOfMarkerParticles(int startIdx, int endIdx, double dt) {
    assert(startIdx <= endIdx);

    MarkerParticle mp;
    glm::vec3 p;
    glm::vec3 vi;
    for (int idx = startIdx; idx <= endIdx; idx++) {
        mp = _markerParticles[idx];

        vi = _MACVelocity.evaluateVelocityAtPosition(mp.position);
        p = _RK4(mp.position, vi, dt);

        if (!_isPositionInGrid(p.x, p.y, p.z)) {
            continue;
        }

        int i, j, k;
        positionToGridIndex(p, &i, &j, &k);

        if (_isCellSolid(i, j, k)) {
            glm::vec3 norm;
            glm::vec3 coll = _calculateSolidCellCollision(mp.position, p, &norm);

            // jog p back a bit from cell face
            p = coll + (float)(0.001*_dx)*norm;

            //assert(!_isCellSolid(i, j, k));
        }

        positionToGridIndex(p, &i, &j, &k);
        if (!_isCellSolid(i, j, k)) {
            _markerParticles[idx].position = p;
            _markerParticles[idx].i = i;
            _markerParticles[idx].j = j;
            _markerParticles[idx].k = k;
        }
        else {
            std::cout << "solid cell: " << " " << p.x << " " << p.y << " " << p.z << " " <<
                mp.position.x << " " << mp.position.y << " " << mp.position.z << std::endl;

        }
    }
    
}

void FluidSimulation::_advanceMarkerParticles(double dt) {
    int size = _markerParticles.size();

    std::vector<int> startIndices;
    std::vector<int> endIndices;

    int numThreads = _numAdvanceMarkerParticleThreads;
    int chunksize = floor(size / numThreads);
    for (int i = 0; i < numThreads; i++) {
        int startIdx = (i == 0) ? 0 : endIndices[i - 1] + 1;
        int endIdx = (i == numThreads - 1) ? size - 1 : startIdx + chunksize - 1;

        startIndices.push_back(startIdx);
        endIndices.push_back(endIdx);
    }

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; i++) {
        threads.push_back(std::thread(&FluidSimulation::_advanceRangeOfMarkerParticles,
                                      this,
                                      startIndices[i],
                                      endIndices[i],
                                      dt));
    }

    for (int i = 0; i < numThreads; i++) {
        threads[i].join();
    }
}

void FluidSimulation::_applyPressureToVelocityField(double dt) {
    _MACVelocity.resetTemporaryVelocityField();

    double usolid = 0.0;   // solids are stationary
    double scale = dt / (_density * _dx);
    double invscale = 1.0 / scale;

    for (int k = 0; k < _k_voxels; k++) {
        for (int j = 0; j < _j_voxels; j++) {
            for (int i = 0; i < _i_voxels + 1; i++) {
                if (_isFaceBorderingMaterialU(i, j, k, M_FLUID)) {
                    double ci = i - 1; double cj = j; double ck = k;

                    double p0, p1;
                    if (!_isCellSolid(ci, cj, ck) && !_isCellSolid(ci + 1, cj, ck)) {
                        p0 = _pressureGrid(ci, cj, ck);
                        p1 = _pressureGrid(ci + 1, cj, ck);
                    } else if (_isCellSolid(ci, cj, ck)) {
                        p0 = _pressureGrid(ci + 1, cj, ck) - 
                             invscale*(_MACVelocity.U(i, j, k) - usolid);
                        p1 = _pressureGrid(ci + 1, cj, ck);
                    } else {
                        p0 = _pressureGrid(ci, cj, ck);
                        p1 = _pressureGrid(ci, cj, ck) +
                             invscale*(_MACVelocity.U(i, j, k) - usolid);
                    }

                    double unext = _MACVelocity.U(i, j, k) - scale*(p1 - p0);
                    _MACVelocity.setTempU(i, j, k, unext);
                }
            }
        }
    }

    for (int k = 0; k < _k_voxels; k++) {
        for (int j = 0; j < _j_voxels + 1; j++) {
            for (int i = 0; i < _i_voxels; i++) {
                if (_isFaceBorderingMaterialV(i, j, k, M_FLUID)) {
                    double ci = i; double cj = j - 1; double ck = k;

                    double p0, p1;
                    if (!_isCellSolid(ci, cj, ck) && !_isCellSolid(ci, cj + 1, ck)) {
                        p0 = _pressureGrid(ci, cj, ck);
                        p1 = _pressureGrid(ci, cj + 1, ck);
                    }
                    else if (_isCellSolid(ci, cj, ck)) {
                        p0 = _pressureGrid(ci, cj + 1, ck) -
                            invscale*(_MACVelocity.V(i, j, k) - usolid);
                        p1 = _pressureGrid(ci, cj + 1, ck);
                    }
                    else {
                        p0 = _pressureGrid(ci, cj, ck);
                        p1 = _pressureGrid(ci, cj, ck) +
                            invscale*(_MACVelocity.V(i, j, k) - usolid);
                    }

                    double vnext = _MACVelocity.V(i, j, k) - scale*(p1 - p0);
                    _MACVelocity.setTempV(i, j, k, vnext);
                }
            }
        }
    }

    for (int k = 0; k < _k_voxels + 1; k++) {
        for (int j = 0; j < _j_voxels; j++) {
            for (int i = 0; i < _i_voxels; i++) {
                if (_isFaceBorderingMaterialW(i, j, k, M_FLUID)) {
                    double ci = i; double cj = j; double ck = k - 1;

                    double p0, p1;
                    if (!_isCellSolid(ci, cj, ck) && !_isCellSolid(ci, cj, ck + 1)) {
                        p0 = _pressureGrid(ci, cj, ck);
                        p1 = _pressureGrid(ci, cj, ck + 1);
                    }
                    else if (_isCellSolid(ci, cj, ck)) {
                        p0 = _pressureGrid(ci, cj, ck + 1) -
                             invscale*(_MACVelocity.W(i, j, k) - usolid);
                        p1 = _pressureGrid(ci, cj, ck + 1);
                    }
                    else {
                        p0 = _pressureGrid(ci, cj, ck);
                        p1 = _pressureGrid(ci, cj, ck) +
                             invscale*(_MACVelocity.W(i, j, k) - usolid);
                    }

                    double wnext = _MACVelocity.W(i, j, k) - scale*(p1 - p0);
                    _MACVelocity.setTempW(i, j, k, wnext);
                }
            }
        }
    }

    _MACVelocity.commitTemporaryVelocityFieldValues();
}

void FluidSimulation::_stepFluid(double dt) {

    StopWatch timer1 = StopWatch();
    StopWatch timer2 = StopWatch();
    StopWatch timer3 = StopWatch();
    StopWatch timer4 = StopWatch();
    StopWatch timer5 = StopWatch();
    StopWatch timer6 = StopWatch();
    StopWatch timer7 = StopWatch();
    StopWatch timer8 = StopWatch();

    std::cout << "--------------------------------------------------" << std::endl;
    std::cout << "Frame: " << _currentFrame <<
                 "\tStep time: " << floor(dt*10000.0) / 10000.0 << "s" << std::endl;
    std::cout << std::endl;

    timer1.start();

    timer2.start();
    _updateFluidCells();
    timer2.stop();

    std::cout << "Update Fluid Cells:          \t" << 
        floor(timer2.getTime()*10000.0) / 10000.0 << "s" << std::endl;
    std::cout << "\tNum Fluid Cells: " << _fluidCellIndices.size() << std::endl;

    timer3.start();
    _extrapolateFluidVelocities();
    timer3.stop();

    std::cout << "Extrapolate Fluid Velocities:\t" << 
        floor(timer3.getTime()*10000.0) / 10000.0 << "s" << std::endl;

    timer4.start();
    _applyBodyForcesToVelocityField(dt);
    timer4.stop();

    std::cout << "Apply Body Forces:           \t" << 
        floor(timer4.getTime()*10000.0) / 10000.0 << "s" << std::endl;

    timer5.start();
    _advectVelocityField(dt);
    timer5.stop();

    std::cout << "Advect Velocity Field:       \t" << 
        floor(timer5.getTime()*10000.0) / 10000.0 << "s" << std::endl;

    timer6.start();
    _updatePressureGrid(dt);
    timer6.stop();

    std::cout << "Update Pressure Grid:        \t" << 
        floor(timer6.getTime()*10000.0) / 10000.0 << "s" << std::endl;

    timer7.start();
    _applyPressureToVelocityField(dt);
    timer7.stop();

    std::cout << "Apply Pressure:              \t" << 
        floor(timer7.getTime()*10000.0) / 10000.0 << "s" << std::endl;

    timer8.start();
    _advanceMarkerParticles(dt);
    timer8.stop();

    std::cout << "Advance Marker Particles:    \t" << 
        floor(timer8.getTime()*10000.0) / 10000.0 << "s" << std::endl;

    timer1.stop();

    double totalTime = floor(timer1.getTime()*1000.0) / 1000.0;
    std::cout << "Simulation Time:           \t" << totalTime << "s" << std::endl;
    std::cout << std::endl;

    double p2 = floor(1000 * timer2.getTime() / totalTime) / 10.0;
    double p3 = floor(1000 * timer3.getTime() / totalTime) / 10.0;
    double p4 = floor(1000 * timer4.getTime() / totalTime) / 10.0;
    double p5 = floor(1000 * timer5.getTime() / totalTime) / 10.0;
    double p6 = floor(1000 * timer6.getTime() / totalTime) / 10.0;
    double p7 = floor(1000 * timer7.getTime() / totalTime) / 10.0;
    double p8 = floor(1000 * timer8.getTime() / totalTime) / 10.0;

    std::cout << "Percentage Breakdown" << std::endl << std::endl;
    std::cout << "Update Fluid Cells:          \t" << p2 << "%" << std::endl;
    std::cout << "Extrapolate Fluid Velocities:\t" << p3 << "%" << std::endl;
    std::cout << "Apply Body Forces:           \t" << p4 << "%" << std::endl;
    std::cout << "Advect Velocity Field:       \t" << p5 << "%" << std::endl;
    std::cout << "Update Pressure Grid:        \t" << p6 << "%" << std::endl;
    std::cout << "Apply Pressure:              \t" << p7 << "%" << std::endl;
    std::cout << "Advance Marker Particles:    \t" << p8 << "%" << std::endl;
    std::cout << std::endl;


}

void FluidSimulation::update(double dt) {
    if (!_isSimulationRunning || !_isSimulationInitialized || !_isFluidInSimulation) {
        return;
    }
    _isCurrentFrameFinished = false;

    int numsteps = 0;
    double timeleft = dt;
    while (timeleft > 0.0) {
        double timestep = _calculateNextTimeStep();
        if (timeleft - timestep < 0.0) {
            timestep = timeleft;
        }
        timeleft -= timestep;
        numsteps++;
        _stepFluid(timestep);
    }

    _currentFrame++;

    _isCurrentFrameFinished = true;
}

void FluidSimulation::draw() {


}