/*
Copyright (c) 2015 Ryan L. Guy

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgement in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
#include "isotropicparticlemesher.h"

IsotropicParticleMesher::IsotropicParticleMesher() {
}

IsotropicParticleMesher::IsotropicParticleMesher(int isize, int jsize, int ksize, double dx) :
								                	_isize(isize), _jsize(jsize), _ksize(ksize), _dx(dx) {

}

IsotropicParticleMesher::~IsotropicParticleMesher() {

}

void IsotropicParticleMesher::setSubdivisionLevel(int n) {
	assert(n >= 1);
	_subdivisionLevel = n;
}

void IsotropicParticleMesher::setNumPolygonizationSlices(int n) {
	assert(n >= 1);

	if (n > _isize) {
		n = _isize;
	}

	_numPolygonizationSlices = n;
}

TriangleMesh IsotropicParticleMesher::meshParticles(FragmentedVector<MarkerParticle> &particles, 
	                                                FluidMaterialGrid &materialGrid,
	                                                double particleRadius) {

	assert(materialGrid.width == _isize &&
		   materialGrid.height == _jsize &&
		   materialGrid.depth == _ksize);
	assert(particleRadius > 0.0);

	_particleRadius = particleRadius;

	if (_numPolygonizationSlices == 1) {
		return _polygonizeAll(particles, materialGrid);
	}

	return _polygonizeSlices(particles, materialGrid);
}

TriangleMesh IsotropicParticleMesher::_polygonizeAll(FragmentedVector<MarkerParticle> &particles, 
	                                                 FluidMaterialGrid &materialGrid) {
	int subd = _subdivisionLevel;
	int width = _isize*subd;
	int height = _jsize*subd;
	int depth = _ksize*subd;
	double dx = _dx / (double)subd;

	ImplicitSurfaceScalarField field(width + 1, height + 1, depth + 1, dx);

	int origsubd = materialGrid.getSubdivisionLevel();
	materialGrid.setSubdivisionLevel(subd);
	field.setMaterialGrid(materialGrid);
	materialGrid.setSubdivisionLevel(origsubd);

	field.setPointRadius(_particleRadius);

    for (unsigned int i = 0; i < particles.size(); i++) {
        field.addPoint(particles[i].position);
    }

    Polygonizer3d polygonizer(&field);
    polygonizer.polygonizeSurface();

    return polygonizer.getTriangleMesh();
}

TriangleMesh IsotropicParticleMesher::_polygonizeSlices(FragmentedVector<MarkerParticle> &particles, 
	                                                    FluidMaterialGrid &materialGrid) {
	int width, height, depth;
	double dx;
	_getSubdividedGridDimensions(&width, &height, &depth, &dx);

	int sliceWidth = ceil((double)width / (double)_numPolygonizationSlices);
	int numSlices = ceil((double)width / (double)sliceWidth);

	if (numSlices == 1) {
		return _polygonizeAll(particles, materialGrid);
	}

	TriangleMesh mesh;
	for (int i = 0; i < numSlices; i++) {
		int startidx = i*sliceWidth;
		int endidx = startidx + sliceWidth - 1;
		endidx = endidx < width ? endidx : width - 1;

		TriangleMesh sliceMesh = _polygonizeSlice(startidx, endidx, particles, materialGrid);

		vmath::vec3 offset = _getSliceGridPositionOffset(startidx, endidx);
		sliceMesh.translate(offset);
		mesh.append(sliceMesh);
	}

	mesh.removeDuplicateVertices();

	return mesh;
}

TriangleMesh IsotropicParticleMesher::_polygonizeSlice(int startidx, int endidx, 
		                                                FragmentedVector<MarkerParticle> &particles, 
	                                                    FluidMaterialGrid &materialGrid) {

	int width, height, depth;
	double dx;
	_getSubdividedGridDimensions(&width, &height, &depth, &dx);

	bool isStartSlice = startidx == 0;
	bool isEndSlice = endidx == width - 1;
	bool isMiddleSlice = !isStartSlice && !isEndSlice;

	int gridWidth = endidx - startidx + 1;
	int gridHeight = height;
	int gridDepth = depth;
	if (isStartSlice || isEndSlice) {
		gridWidth++;
	} else if (isMiddleSlice) {
		gridWidth += 2;
	}

	ImplicitSurfaceScalarField field(gridWidth + 1, gridHeight + 1, gridDepth + 1, dx);
	_computeSliceScalarField(startidx, endidx, particles, materialGrid, field);

	Array3d<bool> mask(gridWidth, gridHeight, gridDepth);
	_getSliceMask(startidx, endidx, mask);

	Polygonizer3d polygonizer(&field);
	polygonizer.setSurfaceCellMask(&mask);
    polygonizer.polygonizeSurface();

    return polygonizer.getTriangleMesh();
}

void IsotropicParticleMesher::_getSubdividedGridDimensions(int *i, int *j, int *k, double *dx) {
	*i = _isize*_subdivisionLevel;
	*j = _jsize*_subdivisionLevel;
	*k = _ksize*_subdivisionLevel;
	*dx = _dx / (double)_subdivisionLevel;
}

void IsotropicParticleMesher::_computeSliceScalarField(int startidx, int endidx, 
	                                                   FragmentedVector<MarkerParticle> &markerParticles,
	                                                   FluidMaterialGrid &materialGrid,
	                                                   ImplicitSurfaceScalarField &field) {
	
	FragmentedVector<vmath::vec3> sliceParticles;
	_getSliceParticles(startidx, endidx, markerParticles, sliceParticles);

	int width, height, depth;
	field.getGridDimensions(&width, &height, &depth);

	FluidMaterialGrid sliceMaterialGrid(width - 1, height - 1, depth - 1);
	_getSliceMaterialGrid(startidx, endidx, materialGrid, sliceMaterialGrid);

	field.setMaterialGrid(sliceMaterialGrid);

	vmath::vec3 fieldOffset = _getSliceGridPositionOffset(startidx, endidx);
	field.setOffset(fieldOffset);
	field.setPointRadius(_particleRadius);

	for (unsigned int i = 0; i < sliceParticles.size(); i++) {
        field.addPoint(sliceParticles[i]);
    }

    _updateScalarFieldSeam(startidx, endidx, field);
}

vmath::vec3 IsotropicParticleMesher::_getSliceGridPositionOffset(int startidx, int endidx) {
	int width, height, depth;
	double dx;
	_getSubdividedGridDimensions(&width, &height, &depth, &dx);

	bool isStartSlice = startidx == 0;

	double offx;
	if (isStartSlice) {
		offx = startidx*dx;
	} else {
		offx = (startidx - 1)*dx;
	}

	return vmath::vec3(offx, 0.0, 0.0);	
}

void IsotropicParticleMesher::_getSliceParticles(int startidx, int endidx, 
	                                             FragmentedVector<MarkerParticle> &markerParticles,
		                                         FragmentedVector<vmath::vec3> &sliceParticles) {
	AABB bbox = _getSliceAABB(startidx, endidx);
	for (unsigned int i = 0; i < markerParticles.size(); i++) {
		if (bbox.isPointInside(markerParticles[i].position)) {
			sliceParticles.push_back(markerParticles[i].position);
		}
	}
}

void IsotropicParticleMesher::_getSliceMaterialGrid(int startidx, int endidx,
		                       FluidMaterialGrid &materialGrid,
		                       FluidMaterialGrid &sliceMaterialGrid) {

	int origsubd = materialGrid.getSubdivisionLevel();
	materialGrid.setSubdivisionLevel(_subdivisionLevel);
	
	Material m;
	for (int k = 0; k < sliceMaterialGrid.depth; k++) {
		for (int j = 0; j < sliceMaterialGrid.height; j++) {
			for (int i = 0; i < sliceMaterialGrid.width; i++) {
				m = materialGrid(startidx + i, j, k);
				sliceMaterialGrid.set(i, j, k, m);
			}
		}
	}

	materialGrid.setSubdivisionLevel(origsubd);
}

AABB IsotropicParticleMesher::_getSliceAABB(int startidx, int endidx) {
	int width, height, depth;
	double dx;
	_getSubdividedGridDimensions(&width, &height, &depth, &dx);

	bool isStartSlice = startidx == 0;
	bool isEndSlice = endidx == width - 1;
	bool isMiddleSlice = !isStartSlice && !isEndSlice;

	double gridWidth = (endidx - startidx + 1)*dx;
	double gridHeight = height*dx;
	double gridDepth = depth*dx;
	if (isStartSlice || isEndSlice) {
		gridWidth += dx;
	} else if (isMiddleSlice) {
		gridWidth += 2.0*dx;
	}

	vmath::vec3 offset = _getSliceGridPositionOffset(startidx, endidx);

	AABB bbox(offset, gridWidth, gridHeight, gridDepth);
	bbox.expand(2.0*_particleRadius);

	return bbox;
}

void IsotropicParticleMesher::_updateScalarFieldSeam(int startidx, int endidx,
	                                                 ImplicitSurfaceScalarField &field) {
	int width, height, depth;
	double dx;
	_getSubdividedGridDimensions(&width, &height, &depth, &dx);

	bool isStartSlice = startidx == 0;
	bool isEndSlice = endidx == width - 1;

	if (!isStartSlice) {
		_applyScalarFieldSliceSeamData(field);
	}
	if (!isEndSlice) {
		_saveScalarFieldSliceSeamData(field);
	}
}

void IsotropicParticleMesher::_applyScalarFieldSliceSeamData(ImplicitSurfaceScalarField &field) {
	int width, height, depth;
	field.getGridDimensions(&width, &height, &depth);

	for (int k = 0; k < depth; k++) {
		for (int j = 0; j < height; j++) {
			for (int i = 0; i <= 2; i++) {
				field.setScalarFieldValue(i, j, k, _scalarFieldSeamData(i, j, k));
			}
		}
	}
}

void IsotropicParticleMesher::_saveScalarFieldSliceSeamData(ImplicitSurfaceScalarField &field) {
	int width, height, depth;
	field.getGridDimensions(&width, &height, &depth);

	_scalarFieldSeamData = Array3d<float>(3, height, depth);
	for (int k = 0; k < depth; k++) {
		for (int j = 0; j < height; j++) {
			for (int i = 0; i <= 2; i++) {
				_scalarFieldSeamData.set(i, j, k, field.getRawScalarFieldValue(i + width - 3, j, k));
			}
		}
	}
}

void IsotropicParticleMesher::_getSliceMask(int startidx, int endidx, Array3d<bool> &mask) {
	mask.fill(true);

	int width, height, depth;
	double dx;
	_getSubdividedGridDimensions(&width, &height, &depth, &dx);

	bool isStartSlice = startidx == 0;
	bool isEndSlice = endidx == width - 1;

	if (!isStartSlice) {
		int idx = 0;
		for (int k = 0; k < mask.depth; k++) {
			for (int j = 0; j < mask.height; j++) {
				mask.set(idx, j, k, false);
			}
		}
	}

	if (!isEndSlice) {
		int idx = mask.width - 1;
		for (int k = 0; k < mask.depth; k++) {
			for (int j = 0; j < mask.height; j++) {
				mask.set(idx, j, k, false);
			}
		}
	}
}