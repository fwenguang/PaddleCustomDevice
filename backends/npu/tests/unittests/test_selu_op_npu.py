#   Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
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

import unittest
import numpy as np
from tests.op_test import OpTest
import paddle
import paddle.fluid as fluid
import paddle.nn.functional as F

paddle.enable_static()


def ref_selu(
    x,
    scale=1.0507009873554804934193349852946,
    alpha=1.6732632423543772848170429916717,
):
    out = np.copy(x)
    out_flat = out.flatten()
    for i in range(out_flat.size):
        if out_flat[i] < 0:
            out_flat[i] = alpha * np.exp(out_flat[i]) - alpha
        out_flat[i] = scale * out_flat[i]
    out = out_flat.reshape(x.shape)
    return out


class SeluTest(OpTest):
    def set_npu(self):
        self.__class__.use_custom_device = True
        self.place = paddle.CustomPlace("npu", 0)

    def setUp(self):
        self.set_npu()
        self.op_type = "selu"
        self.python_api = paddle.nn.functional.selu
        self.init_x_shape()
        self.init_dtype()

        alpha = 1.6732632423543772848170429916717
        scale = 1.0507009873554804934193349852946

        x = np.random.normal(size=self.x_shape).astype(self.dtype)

        # Since zero point in selu is not differentiable, avoid randomize
        # zero.
        x[np.abs(x) < 0.005] = 0.02

        out = ref_selu(x, scale, alpha)

        self.inputs = {"X": x}
        self.outputs = {"Out": out}

        self.attrs = {
            "alpha": alpha,
            "scale": scale,
        }

    def init_x_shape(self):
        self.x_shape = [3, 5, 5, 10]

    def init_dtype(self):
        self.dtype = np.float32

    def test_check_output(self):
        self.check_output_with_place(self.place)

    def test_check_grad(self):
        self.check_grad_with_place(self.place, ["X"], "Out")


class SeluTestFP16(SeluTest):
    def init_dtype(self):
        self.dtype = np.float16

    def test_check_output(self):
        self.check_output_with_place(self.place, atol=1e-3)

    def test_check_grad(self):
        pass


class TestSeluAPI(unittest.TestCase):
    # test paddle.nn.SELU, paddle.nn.functional.selu
    def setUp(self):
        self.scale = 1.5
        self.alpha = 2.0
        self.x_np = np.random.normal(size=[3, 5, 5, 10]).astype(np.float64)
        # Since zero point in selu is not differentiable, avoid randomize
        # zero.
        self.x_np[np.abs(self.x_np) < 0.005] = 0.02
        self.place = paddle.CustomPlace("npu", 0)

    def test_static_api(self):
        paddle.enable_static()
        with paddle.static.program_guard(paddle.static.Program()):
            x = paddle.static.data("X", self.x_np.shape, self.x_np.dtype)
            out1 = F.selu(x, self.scale, self.alpha)
            selu = paddle.nn.SELU(self.scale, self.alpha)
            out2 = selu(x)
            exe = paddle.static.Executor(self.place)
            res = exe.run(feed={"X": self.x_np}, fetch_list=[out1, out2])
        out_ref = ref_selu(self.x_np, self.scale, self.alpha)
        for r in res:
            np.testing.assert_allclose(out_ref, r, rtol=1e-05)

    def test_dygraph_api(self):
        paddle.disable_static(self.place)
        x = paddle.to_tensor(self.x_np)
        out1 = F.selu(x, self.scale, self.alpha)
        selu = paddle.nn.SELU(self.scale, self.alpha)
        out2 = selu(x)
        out_ref = ref_selu(self.x_np, self.scale, self.alpha)
        for r in [out1, out2]:
            np.testing.assert_allclose(out_ref, r.numpy(), rtol=1e-05)
        paddle.enable_static()

    def test_fluid_api(self):
        paddle.enable_static()
        with fluid.program_guard(fluid.Program()):
            x = paddle.static.data("X", self.x_np.shape, self.x_np.dtype)
            out = F.selu(x, self.scale, self.alpha)
            exe = fluid.Executor(self.place)
            res = exe.run(feed={"X": self.x_np}, fetch_list=[out])
        out_ref = ref_selu(self.x_np, self.scale, self.alpha)
        np.testing.assert_allclose(out_ref, res[0], rtol=1e-05)


if __name__ == "__main__":
    unittest.main()
