import copy
import pyarts.arts as cxx

import xarray
import numpy as np
from scipy import interpolate

def extract_slice(g, s=slice(None), axis=0):
    """
    Return a new gridded field containing a slice of the current one.
    
    Parameters
    ----------
    s : slice
        Slice.
    axis : int
        Axis to slice along.
    
    Returns
    -------
    gf : GriddedField1 or GriddedField2 or GriddedField3 or GriddedField4 or GriddedField5 or GriddedField6
        Gridded field containing sliced grids and data.
    """
    g.checksize_strict()
    
    gf = copy.deepcopy(g)
    axis_grid = gf.get_grid(axis)
    if isinstance(axis_grid, cxx.Vector):
        gf.set_grid(axis, axis_grid[s])
    else:
        gf.set_grid(axis, type(axis_grid)(list(axis_grid)[s]))
    slices = [slice(None)] * gf.dim
    slices[axis] = s
    gf.data = gf.data[tuple(slices)]

    return gf


# NOTE: We cannot safely pass kwargs to C++ and then to python again, so we pass them as dict
def refine_grid(gin, new_grid, axis=0, type="linear", hidden_kwargs={}):
    """
    Interpolate GriddedField axis to a new grid.
    
    This function replaces a grid of a GriddField and interpolates all
    data to match the new coordinates. :func:`scipy.interpolate.interp1d`
    is used for interpolation.
    
    Parameters
    ----------
    new_grid : numpy.ndarray
        The coordinates of the interpolated values.
    axis : int
        Specifies the axis of data along which to interpolate.
        Interpolation defaults to the first axis of the gridded field.
    type : str or function
        Rescaling type for function if str or rescaling function
    Returns
    ------
    gf : GriddedField1 or GriddedField2 or GriddedField3 or GriddedField4 or GriddedField5 or GriddedField6
        gridded field
    """
    if type == "linear":
        fun = np.array
    elif type == "log10":
        fun = np.log10
    elif type == "log":
        fun = np.log
    else:
        fun = type
        
    g = copy.deepcopy(gin)
    
    if len(g.get_grid(axis)) > 1:
        f = interpolate.interp1d(
            fun(g.get_grid(axis)), g.data, axis=axis, **hidden_kwargs)
        g.set_grid(axis, new_grid)
        g.data = f(fun(new_grid))
    else:  # if the intention is to create a useful TensorX
        g.data = g.data.repeat(len(new_grid), axis=axis)
        g.data = f(fun(new_grid))

    g.checksize_strict()

    return g


def to_xarray(g):
    """Convert gridded field to :class:`xarray.DataArray` object.
    
    Convert a gridded field object into a :class:`xarray.DataArray`
    object.  The dataname is used as the :class:`~xarray.DataArray` name.
    
    Returns
    -------
    da : xarray.DataArray
        Object corresponding to gridded field
    """

    da = xarray.DataArray(g.data)
    da = da.rename(dict((k, v)
        for (k, v) in zip(da.dims, [str(g.get_grid_name(i)) for i in range(g.dim)])
        if v!=""))
    da = da.assign_coords(
        **{name: coor
            for (name, coor) in zip(da.dims, [np.array(g.get_grid(i)) for i in range(g.dim)])
            if len(coor)>0})
    if g.name: da.attrs['data_name'] = str(g.name)
    return da


def from_xarray(cls, da):
    """Create gridded field from a :class:`xarray.DataArray` object.
    
    The data and its dimensions are returned as a gridded field object.
    The :class:`~xarray.DataArray` name is used as name for the gridded field. If the attribute
    `data_name` is present, it is used as `dataname` on the gridded field.
    
    Parameters
    ----------
    da : xarray.DataArray
        :class:`xarray.DataArray` containing the dimensions and data.
    
    Returns
    -------
    gf : GriddedField1 or GriddedField2 or GriddedField3 or GriddedField4 or GriddedField5 or GriddedField6
        Gridded field object.
    """
    obj = cls()
    for i in range(obj.dim):
        obj.set_grid(i, da[da.dims[i]].values)
        obj.set_grid_name(i, da.dims[i])
    if da.values.ndim != obj.dim:
        raise RuntimeError(f"Dimension mismatch: Expected {obj.dim} got {da.values.ndim}")
    obj.data = da.values
    obj.name = str(da.attrs.get('data_name', 'Data'))
    obj.checksize_strict()
    return obj


def to_dict(self):
    """Convert gridded field to :class:`dict`.
    
    Converts a gridded field object into a classic Python dictionary. The
    gridname is used as dictionary key. If the grid is unnamed the key is
    generated automatically ('grid1', 'grid2', ...). The data can be
    accessed through the 'data' key.
    
    Returns
    -------
    pydict : dict
        Dictionary containing the grids and data.
    """
    grids, gridnames = self.grids, self.gridnames

    if gridnames is None:
        gridnames = ['grid%d' % n for n in range(1, self.dimension + 1)]

    for n, name in enumerate(gridnames):
        if name == '':
            gridnames[n] = 'grid%d' % (n + 1)

    d = {name: grid for name, grid in zip(gridnames, grids)}

    d['data'] = self.data

    return d


getattr(cxx, "_detailsGriddedField").extract_slice = extract_slice
getattr(cxx, "_detailsGriddedField").refine_grid = refine_grid
getattr(cxx, "_detailsGriddedField").from_xarray = from_xarray
getattr(cxx, "_detailsGriddedField").to_xarray = to_xarray
getattr(cxx, "_detailsGriddedField").to_dict = to_dict
