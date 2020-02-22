// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Copyright (C) 2019 Intel Corporation

#include "test_precomp.hpp"
#include <string>
#include <utility>

namespace opencv_test
{

namespace ThisTest
{
using GPointOpaque = cv::GOpaque<cv::Point>;

G_TYPED_KERNEL(GeneratePoint, <GPointOpaque(GMat)>, "test.opaque.gen_point")
{
    static GOpaqueDesc outMeta(const GMatDesc&) { return empty_gopaque_desc(); }
};

G_TYPED_KERNEL(FillMat, <GMat(cv::GOpaque<int>, int, int, cv::Size)>, "test.opaque.fill_mat")
{
    static GMatDesc outMeta(const GOpaqueDesc&, int depth, int chan, cv::Size size)
    {
        return cv::GMatDesc{depth, chan, size};
    }
};

G_TYPED_KERNEL(PaintPoint, <GMat(GPointOpaque, int, int, cv::Size)>, "test.opaque.paint_point")
{
    static GMatDesc outMeta(const GOpaqueDesc&, int depth, int chan, cv::Size size)
    {
        return cv::GMatDesc{depth, chan, size};
    }
};

struct MyCustomType{
    int num;
    std::string s;
};

using GOpaq2 = std::tuple<GOpaque<MyCustomType>,GOpaque<MyCustomType>>;

G_TYPED_KERNEL_M(GenerateOpaque, <GOpaq2(GMat, GMat, std::string)>, "test.opaque.gen_point_multy")
{
    static std::tuple<GOpaqueDesc, GOpaqueDesc> outMeta(const GMatDesc&, const GMatDesc&, std::string)
    {
        return std::make_tuple(empty_gopaque_desc(), empty_gopaque_desc());
    }
};

} // namespace ThisTest

namespace
{
GAPI_OCV_KERNEL(OCVGeneratePoint, ThisTest::GeneratePoint)
{
    static void run(const cv::Mat&, cv::Point& out)
    {
        out = cv::Point(42, 42);
    }
};

GAPI_OCV_KERNEL(OCVFillMat, ThisTest::FillMat)
{
    static void run(int a, int, int, cv::Size, cv::Mat& out)
    {
        out = cv::Scalar(a);
    }
};

GAPI_OCV_KERNEL(OCVPaintPoint, ThisTest::PaintPoint)
{
    static void run(cv::Point a, int, int, cv::Size, cv::Mat& out)
    {
        out.at<uint8_t>(a) = 77;
    }
};

GAPI_OCV_KERNEL(OCVGenerateOpaque, ThisTest::GenerateOpaque)
{
    static void run(const cv::Mat& a, const cv::Mat& b, const std::string& s,
                    ThisTest::MyCustomType &out1, ThisTest::MyCustomType &out2)
    {
        out1.num = a.size().width * a.size().height;
        out1.s = s;

        out2.num = b.size().width * b.size().height;
        auto s2 = s;
        std::reverse(s2.begin(), s2.end());
        out2.s = s2;
    }
};
} // (anonymous namespace)

TEST(GOpaque, TestOpaqueOut)
{
    cv::Mat input = cv::Mat(52, 52, CV_8U);
    cv::Point point;

    cv::GMat in;
    auto out = ThisTest::GeneratePoint::on(in);

    cv::GComputation c(cv::GIn(in), cv::GOut(out));
    c.apply(cv::gin(input), cv::gout(point), cv::compile_args(cv::gapi::kernels<OCVGeneratePoint>()));

    EXPECT_TRUE(point == cv::Point(42, 42));
}

TEST(GOpaque, TestOpaqueIn)
{
    cv::Size sz = {42, 42};
    int depth = CV_8U;
    int chan = 1;
    cv::Mat mat = cv::Mat(sz, CV_MAKETYPE(depth, chan));
    int fill = 0;

    cv::GOpaque<int> in;
    auto out = ThisTest::FillMat::on(in, depth, chan, sz);

    cv::GComputation c(cv::GIn(in), cv::GOut(out));
    c.apply(cv::gin(fill), cv::gout(mat), cv::compile_args(cv::gapi::kernels<OCVFillMat>()));

    auto diff = cv::Mat(sz, CV_MAKETYPE(depth, chan), cv::Scalar(fill)) - mat;
    EXPECT_EQ(cv::countNonZero(diff), 0);
}

TEST(GOpaque, TestOpaqueBetween)
{
    cv::Size sz = {50, 50};
    int depth = CV_8U;
    int chan = 1;
    cv::Mat mat_in = cv::Mat::zeros(sz, CV_MAKETYPE(depth, chan));
    cv::Mat mat_out = cv::Mat::zeros(sz, CV_MAKETYPE(depth, chan));

    cv::GMat in, out;
    auto betw = ThisTest::GeneratePoint::on(in);
    out = ThisTest::PaintPoint::on(betw, depth, chan, sz);

    cv::GComputation c(cv::GIn(in), cv::GOut(out));
    c.apply(cv::gin(mat_in), cv::gout(mat_out), cv::compile_args(cv::gapi::kernels<OCVGeneratePoint, OCVPaintPoint>()));

    int painted = mat_out.at<uint8_t>(42, 42);
    EXPECT_EQ(painted, 77);
}

TEST(GOpaque, TestOpaqueCustomOut2)
{
    cv::Mat input1 = cv::Mat(52, 52, CV_8U);
    cv::Mat input2 = cv::Mat(42, 42, CV_8U);
    std::string str = "opaque";
    std::string str2 = str;
    std::reverse(str2.begin(), str2.end());

    ThisTest::MyCustomType out1, out2;

    cv::GMat in1, in2;
    auto out = ThisTest::GenerateOpaque::on(in1, in2, str);

    cv::GComputation c(cv::GIn(in1, in2), cv::GOut(std::get<0>(out), std::get<1>(out)));
    c.apply(cv::gin(input1, input2), cv::gout(out1, out2), cv::compile_args(cv::gapi::kernels<OCVGenerateOpaque>()));

    EXPECT_EQ(out1.num, input1.size().width * input1.size().height);
    EXPECT_EQ(out1.s, str);

    EXPECT_EQ(out2.num, input2.size().width * input2.size().height);
    EXPECT_EQ(out2.s, str2);
}

TEST(GOpaque_OpaqueRef, TestMov)
{
    // Warning: this test is testing some not-very-public APIs
    // Test how OpaqueRef's mov() (aka poor man's move()) is working.

    using I = std::string;

    std::string str = "this string must be long due to short string optimization";
    const I gold(str);

    I test = gold;
    const char* ptr = test.data();

    cv::detail::OpaqueRef ref(test);
    cv::detail::OpaqueRef mov;
    mov.reset<I>();

    EXPECT_EQ(gold, ref.rref<I>());         // ref = gold

    mov.mov(ref);
    EXPECT_EQ(gold, mov.rref<I>());         // mov obtained the data
    EXPECT_EQ(ptr,  mov.rref<I>().data());  // pointer is unchanged (same data)
    EXPECT_EQ(test, ref.rref<I>());         // ref = test
    EXPECT_NE(test, mov.rref<I>());         // ref lost the data
}
} // namespace opencv_test
