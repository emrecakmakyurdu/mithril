# Copyright 2022 Synnada, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import ctypes

from ....cores.c.array import PyArray
from ....cores.c.ggml.ggml_core import ggml_struct
from ....cores.c.raw_c.array import Array, lib, zeros

__all__ = ["add", "multiplication"]


def add(left: PyArray, right: PyArray) -> PyArray:
    # In C backend, output is given as first input
    output = zeros(left.shape)
    left_c_1_ptr = ctypes.cast(left.arr.data, ctypes.POINTER(ctypes.c_float))
    left_c = PyArray(Array(data=left_c_1_ptr), left.shape)
    right_c_1_ptr = ctypes.cast(right.arr.data, ctypes.POINTER(ctypes.c_float))
    right_c = PyArray(Array(data=right_c_1_ptr), right.shape)
    lib.add(
        ctypes.byref(output.arr), ctypes.byref(left_c.arr), ctypes.byref(right_c.arr)
    )
    _shape = output.shape
    data_ptr = ctypes.cast(output.arr.data, ctypes.c_void_p)
    return PyArray(ggml_struct(data=data_ptr), _shape)


def multiplication(left: PyArray, right: PyArray) -> PyArray:
    # In C backend, output is given as first input
    output = zeros(left.shape)
    left_c_1_ptr = ctypes.cast(left.arr.data, ctypes.POINTER(ctypes.c_float))
    left_c = PyArray(Array(data=left_c_1_ptr), left.shape)
    right_c_1_ptr = ctypes.cast(right.arr.data, ctypes.POINTER(ctypes.c_float))
    right_c = PyArray(Array(data=right_c_1_ptr), right.shape)
    lib.multiplication(
        ctypes.byref(output.arr), ctypes.byref(left_c.arr), ctypes.byref(right_c.arr)
    )
    _shape = output.shape
    data_ptr = ctypes.cast(output.arr.data, ctypes.c_void_p)
    return PyArray(ggml_struct(data=data_ptr), _shape)


primitive_func_dict = {key: fn for key, fn in globals().items() if callable(fn)}
