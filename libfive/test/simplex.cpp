/*
libfive: a CAD kernel for modeling with implicit functions
Copyright (C) 2018  Matt Keeter

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this file,
You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "catch.hpp"

#include "libfive/render/brep/simplex/simplex_pool.hpp"
#include "libfive/render/brep/simplex/simplex_mesher.hpp"
#include "libfive/render/brep/simplex/simplex_debug.hpp"
#include "libfive/render/brep/indexes.hpp"
#include "libfive/render/brep/dual.hpp"

#include "util/shapes.hpp"
#include "util/mesh_checks.hpp"

using namespace Kernel;

TEST_CASE("SimplexPool")
{
    auto c = circle(1);
    auto r = Region<2>({-1, -1}, {1, 1});

    auto t = SimplexTreePool<2>::build(c, r, 0.1, 1e-8, 1);
    REQUIRE(t.get() != nullptr);

    t->assignIndices();
}

TEST_CASE("SimplexTree<2>::assignIndices")
{
    auto c = circle(1);
    auto r = Region<2>({-1, -1}, {1, 1});

    auto t = SimplexTreePool<2>::build(c, r, 1.1, 1e-8, 1);
    REQUIRE(t.get() != nullptr);

    t->assignIndices();

    REQUIRE(t->isBranch());
    std::set<uint64_t> indices;
    for (auto& c : t->children) {
        CAPTURE(c.load()->region.lower);
        CAPTURE(c.load()->region.upper);

        REQUIRE(c.load() != nullptr);
        REQUIRE(!c.load()->isBranch());
        REQUIRE(c.load()->leaf != nullptr);
        for (auto& i : c.load()->leaf->sub)
        {
            indices.insert(i.load()->index.load());
        }
    }
    REQUIRE(indices.size() == 25);
    REQUIRE(*indices.begin() == 1);
    REQUIRE(*indices.rbegin() == 25);
}

TEST_CASE("SimplexTree<2>: cell collapsing")
{
    SECTION("Rotated box") {
        auto shape = rotate2d(rectangle(-0.5, -0.5, 0.6, 0.6),
                              M_PI / 4);
        auto r = Region<2>({-1, -1}, {1, 1});
        auto t = SimplexTreePool<2>::build(shape, r,
             0.4 /* min_feature */,
             1e-8, /* max_err */
             1 /* workers */);
        REQUIRE(!t->isBranch());
    }
}

TEST_CASE("SimplexTree<3>: types")
{
    auto c = sphere(0.5);
    auto r = Region<3>({-1, -1, -1}, {1, 1, 1});

    auto t = SimplexTreePool<3>::build(c, r);
    REQUIRE(t.get() != nullptr);

    REQUIRE(t->isBranch());
    for (auto& c : t->children) {
        REQUIRE(c.load()->type == Interval::AMBIGUOUS);
    }
}

void CHECK_CORNER_POSITIONS(const SimplexTree<3>* ptr, Region<3> r)
{
    std::list<std::pair<const SimplexTree<3>*, Region<3>>> todo;
    todo.push_back({ptr, r});

    unsigned checked_count = 0;
    while (todo.size())
    {
        auto task = todo.front();
        todo.pop_front();

        if (task.first->isBranch())
        {
            auto rs = task.second.subdivide();
            for (unsigned i=0; i < 8; ++i) {
                todo.push_back({task.first->children[i].load(), rs[i]});
            }
        }
        else if (task.first->leaf)
        {
            for (unsigned i=0; i < 8; ++i) {
                Eigen::Vector3d vt =
                    task.first->leaf->sub[CornerIndex(i).neighbor().i].load()
                                    ->vert;
                Eigen::Vector3d vr = task.second.corner(i);
                CAPTURE(vt);
                CAPTURE(vr);
                REQUIRE((vt - vr).norm() < 1e-6);
                checked_count++;
            }
        }
    }
    REQUIRE(checked_count > 0);
}

TEST_CASE("SimplexTree<3>: Corner positions")
{
    SECTION("Single-cell sphere")
    {
        auto c = sphere(0.5);
        auto r = Region<3>({-1, -1, -1}, {1, 1, 1});
        auto t = SimplexTreePool<3>::build(c, r, 2);

        REQUIRE(t.get() != nullptr);
        REQUIRE(t->type == Interval::AMBIGUOUS);
        REQUIRE(!t->isBranch());
        REQUIRE(t->leaf != nullptr);

        for (unsigned i=0; i < 26; ++i) {
            REQUIRE(t->leaf->sub[i].load()->inside == false);
        }
        REQUIRE(t->leaf->sub[26].load()->inside == true);

        REQUIRE(t->leaf->sub[0].load()->vert ==  Eigen::RowVector3d(-1, -1, -1));
        REQUIRE(t->leaf->sub[1].load()->vert ==  Eigen::RowVector3d( 1, -1, -1));
        REQUIRE(t->leaf->sub[2].load()->vert ==  Eigen::RowVector3d( 0, -1, -1));

        REQUIRE(t->leaf->sub[3].load()->vert ==  Eigen::RowVector3d(-1,  1, -1));
        REQUIRE(t->leaf->sub[4].load()->vert ==  Eigen::RowVector3d( 1,  1, -1));
        REQUIRE(t->leaf->sub[5].load()->vert ==  Eigen::RowVector3d( 0,  1, -1));

        REQUIRE(t->leaf->sub[6].load()->vert ==  Eigen::RowVector3d(-1,  0, -1));
        REQUIRE(t->leaf->sub[7].load()->vert ==  Eigen::RowVector3d( 1,  0, -1));
        REQUIRE(t->leaf->sub[8].load()->vert ==  Eigen::RowVector3d( 0,  0, -1));

        REQUIRE(t->leaf->sub[9].load()->vert ==  Eigen::RowVector3d(-1, -1,  1));
        REQUIRE(t->leaf->sub[10].load()->vert == Eigen::RowVector3d( 1, -1,  1));
        REQUIRE(t->leaf->sub[11].load()->vert == Eigen::RowVector3d( 0, -1,  1));

        REQUIRE(t->leaf->sub[12].load()->vert == Eigen::RowVector3d(-1,  1,  1));
        REQUIRE(t->leaf->sub[13].load()->vert == Eigen::RowVector3d( 1,  1,  1));
        REQUIRE(t->leaf->sub[14].load()->vert == Eigen::RowVector3d( 0,  1,  1));

        REQUIRE(t->leaf->sub[15].load()->vert == Eigen::RowVector3d(-1,  0,  1));
        REQUIRE(t->leaf->sub[16].load()->vert == Eigen::RowVector3d( 1,  0,  1));
        REQUIRE(t->leaf->sub[17].load()->vert == Eigen::RowVector3d( 0,  0,  1));

        REQUIRE(t->leaf->sub[18].load()->vert == Eigen::RowVector3d(-1, -1,  0));
        REQUIRE(t->leaf->sub[19].load()->vert == Eigen::RowVector3d( 1, -1,  0));
        REQUIRE(t->leaf->sub[20].load()->vert == Eigen::RowVector3d( 0, -1,  0));

        REQUIRE(t->leaf->sub[21].load()->vert == Eigen::RowVector3d(-1,  1,  0));
        REQUIRE(t->leaf->sub[22].load()->vert == Eigen::RowVector3d( 1,  1,  0));
        REQUIRE(t->leaf->sub[23].load()->vert == Eigen::RowVector3d( 0,  1,  0));

        REQUIRE(t->leaf->sub[24].load()->vert == Eigen::RowVector3d(-1,  0,  0));
        REQUIRE(t->leaf->sub[25].load()->vert == Eigen::RowVector3d( 1,  0,  0));
        REQUIRE(t->leaf->sub[26].load()->vert == Eigen::RowVector3d( 0,  0,  0));
    }

    SECTION("Sphere")
    {
        auto c = sphere(0.5);
        auto r = Region<3>({-1, -1, -1}, {1, 1, 1});

        auto t = SimplexTreePool<3>::build(c, r);
        REQUIRE(t.get() != nullptr);
        CHECK_CORNER_POSITIONS(t.get(), r);
    }

    SECTION("Box (low-resolution)")
    {
        auto c = box({-0.4, -0.4, -0.4}, {0.4, 0.4, 0.4});
        auto r = Region<3>({-1, -1, -1}, {1, 1, 1});

        auto t = SimplexTreePool<3>::build(c, r, 0.4, 0, 1);
        CHECK_CORNER_POSITIONS(t.get(), r);
    }

}

TEST_CASE("SimplexMesher<3>: box with problematic edges")
{
    auto shape = box({-1, -1, -1}, {1.1, 1.1, 1.1});
    auto r = Region<3>({-2, -2, -2}, {2, 2, 2});

    // Build a tree without any collapsing
    auto t = SimplexTreePool<3>::build(shape, r, 1, -1);
    CHECK_CORNER_POSITIONS(t.get(), r);

    t->assignIndices();

    std::atomic_bool cancel(false);
    auto m = Dual<3>::walk<SimplexMesher>(t, 8,
            cancel, EMPTY_PROGRESS_CALLBACK, shape);

    REQUIRE(m->branes.size() > 0);
    REQUIRE(m->verts.size() > 1);

    PointEvaluator eval(shape);
    for (auto& tri: m->branes) {
        auto a = m->verts[tri[0]];
        auto b = m->verts[tri[1]];
        auto c = m->verts[tri[2]];

        CAPTURE(a);
        CAPTURE(b);
        CAPTURE(c);

        REQUIRE(eval.eval(a.template cast<float>()) == Approx(0.0).margin(1e-4));
        REQUIRE(eval.eval(b.template cast<float>()) == Approx(0.0).margin(1e-4));
        REQUIRE(eval.eval(c.template cast<float>()) == Approx(0.0).margin(1e-4));

        REQUIRE(eval.eval(((a + b) / 2).template cast<float>()) == Approx(0.0).margin(1e-4));
        REQUIRE(eval.eval(((b + c) / 2).template cast<float>()) == Approx(0.0).margin(1e-4));
        REQUIRE(eval.eval(((a + c) / 2).template cast<float>()) == Approx(0.0).margin(1e-4));
    }
}

TEST_CASE("SimplexMesher<3>: tricky shape")
{
    auto b = max(box({-1, 0, -1}, {1, 2, 1}),
                -box({0, 0, -1}, {1, 2, 0}));

    Region<3> r({-5, -5, -5}, {5, 5, 5});
    auto t = SimplexTreePool<3>::build(b, r, 0.25, 1e-8);
    t->assignIndices();

    std::atomic_bool cancel(false);
    auto m = Dual<3>::walk<SimplexMesher>(t, 8,
            cancel, EMPTY_PROGRESS_CALLBACK, b);
    CHECK_EDGE_PAIRS(*m);
}

TEST_CASE("SimplexMesher<3>: another tricky shape", "[!mayfail]")
{
    auto b = max(box({-1, 0, -1}, {1, 2, 1}),
                -box({0, -1, -0.5}, {1, 2.5, 1}));

    Region<3> r({-5, -5, -5}, {5, 5, 5});
    auto t = SimplexTreePool<3>::build(b, r, 0.25, 1e-8);
    t->assignIndices();

    std::atomic_bool cancel(false);
    auto m = Dual<3>::walk<SimplexMesher>(t, 8,
            cancel, EMPTY_PROGRESS_CALLBACK, b);
    CHECK_EDGE_PAIRS(*m);
}

TEST_CASE("SimplexTree<3>: meshing + cell collapsing")
{
    auto c = box({-3.1, -3.1, -3.1}, {3.1, 3.1, 3.1});
    Region<3> r({-4, -4, -4}, {4, 4, 4});

    auto t = SimplexTreePool<3>::build(c, r, 0.9, 1e-8, 8);
    t->assignIndices();

    std::list<const SimplexTree<3>*> todo;
    todo.push_back(t.get());

    std::atomic_bool cancel(false);
    auto m = Dual<3>::walk<SimplexMesher>(t, 8,
            cancel, EMPTY_PROGRESS_CALLBACK, c);

    REQUIRE(m->branes.size() > 0);
    REQUIRE(m->verts.size() > 1);
    CHECK_EDGE_PAIRS(*m);
}

TEST_CASE("SimplexTree<3>::assignIndices")
{
    SECTION("Sphere") {
        auto c = sphere(0.5);
        auto r = Region<3>({-1, -1, -1}, {1, 1, 1});

        auto t = SimplexTreePool<3>::build(c, r, 1.1, 1e-8, 1);
        REQUIRE(t.get() != nullptr);

        t->assignIndices();

        REQUIRE(t->isBranch());
        std::set<uint64_t> indices;
        for (auto& c : t->children) {
            REQUIRE(c.load() != nullptr);
            REQUIRE(!c.load()->isBranch());
            REQUIRE(c.load()->leaf != nullptr);
            for (auto& i : c.load()->leaf->sub)
            {
                indices.insert(i.load()->index.load());
            }
        }
        REQUIRE(indices.size() == 125);
        REQUIRE(*indices.begin() == 1);
        REQUIRE(*indices.rbegin() == 125);
    }


    SECTION("Sphere-cube intersection") {
        auto c = min(sphere(0.7, {0, 0, 0.1}), box({-1, -1, -1}, {1, 1, 0.1}));
        Region<3> r({-10, -10, -10}, {10, 10, 10});

        auto t = SimplexTreePool<3>::build(c, r, 1, 1e-8, 1);
        t->assignIndices();

        std::list<const SimplexTree<3>*> todo;
        todo.push_back(t.get());

        while (todo.size()) {
            const auto next = todo.front();
            todo.pop_front();

            if (next->isBranch()) {
                for (const auto& c: next->children) {
                    todo.push_back(c.load());
                }
            } else if (next->leaf) {
                for (const auto& sub: next->leaf->sub) {
                    REQUIRE(sub.load()->index.load() != 0);
                }
            }
        }
    }
}

TEST_CASE("SimplexTree<3>::leafLevel")
{
    auto c = sphere(0.5);
    auto r = Region<3>({-1, -1, -1}, {1, 1, 1});

    auto t = SimplexTreePool<3>::build(c, r, 1.1, 1e-8, 1);
    REQUIRE(t->isBranch());
    for (auto& c : t->children) {
        REQUIRE(c.load()->leafLevel() == 0);
    }
}

TEST_CASE("SimplexMesher (smoke test)")
{
    std::atomic_bool cancel(false);

    SECTION("High-resolution sphere")
    {
        auto r = Region<3>({-1, -1, -1}, {1, 1, 1});
        auto c = sphere(0.5);
        auto t = SimplexTreePool<3>::build(c, r, 1.1, 1e-8, 1);
        REQUIRE(t->isBranch());
        for (auto& c : t->children) {
            REQUIRE(c.load()->type == Interval::AMBIGUOUS);
        }
        t->assignIndices();

        auto m = Dual<3>::walk<SimplexMesher>(t, 8,
                cancel, EMPTY_PROGRESS_CALLBACK, c);

        REQUIRE(m->branes.size() > 0);
        REQUIRE(m->verts.size() > 1);
    }

    SECTION("Low-resolution sphere")
    {
        auto r = Region<3>({-1, -1, -1}, {1, 1, 1});
        auto c = sphere(0.1);
        auto t = SimplexTreePool<3>::build(c, r, 1);
        t->assignIndices();

        auto m = Dual<3>::walk<SimplexMesher>(t, 8,
                cancel, EMPTY_PROGRESS_CALLBACK, c);
        REQUIRE(m->verts.size() > 1);
        REQUIRE(m->branes.size() > 1);
    }

    SECTION("Low-resolution box")
    {
        auto c = box({-0.4, -0.4, -0.4}, {0.4, 0.4, 0.4});
        auto r = Region<3>({-1, -1, -1}, {1, 1, 1});

        auto t = SimplexTreePool<3>::build(c, r, 0.4, 0, 1);
        t->assignIndices();

        auto m = Dual<3>::walk<SimplexMesher>(t, 8,
                cancel, EMPTY_PROGRESS_CALLBACK, c);
        REQUIRE(m->verts.size() > 1);
        REQUIRE(m->branes.size() > 1);
    }
}

TEST_CASE("SimplexMesher: sphere-box intersection vertex placement")
{
    auto c = min(sphere(0.7, {0, 0, 0.1}), box({-1, -1, -1}, {1, 1, 0.1}));
    Region<3> r({-10, -10, -10}, {10, 10, 10});

    auto t = SimplexTreePool<3>::build(c, r, 0.2, 1e-8, 8);
    t->assignIndices();

    std::atomic_bool cancel(false);
    auto m = Dual<3>::walk<SimplexMesher>(t, 8,
            cancel, EMPTY_PROGRESS_CALLBACK, c);

    // We pick out any triangle that's at the sphere-cube intersection,
    // and check that the intersection vertices are at the sphere's radius.
    auto is_top_surface = [&](uint32_t i) {
        const auto z = m->verts[i].z();
        return z > 0.099 && z < 0.101;
    };
    auto is_above_surface = [&](uint32_t i) {
        return m->verts[i].z() > 0.101;
    };
    auto is_intersection_triangle = [&](Eigen::Matrix<uint32_t, 3, 1> tri)
    {
        return (is_top_surface(tri[0]) ||
                is_top_surface(tri[1]) ||
                is_top_surface(tri[2])) &&
               (is_above_surface(tri[0]) ||
                is_above_surface(tri[1]) ||
                is_above_surface(tri[2]));
    };
    for (auto& tri : m->branes) {
        if (is_intersection_triangle(tri)) {
            for (unsigned i=0; i < 3; ++i) {
                if (is_top_surface(tri[i])) {
                    CAPTURE(m->verts[tri[i]]);
                    REQUIRE(m->verts[tri[i]].head<2>().norm() ==
                            Approx(0.7).margin(0.01));
                }
            }
        }
    }
}

TEST_CASE("SimplexMesher: edge pairing")
{
    std::atomic_bool cancel(false);
    const unsigned workers = 8;

    SECTION("Sphere (low resolution)")
    {
        auto c = sphere(0.4);
        auto r = Region<3>({-1, -1, -1}, {1, 1, 1});

        auto t = SimplexTreePool<3>::build(c, r, 1.1, 0, 1);
        REQUIRE(t->isBranch());
        for (auto& c : t->children) {
            REQUIRE(!c.load()->isBranch());
            REQUIRE(c.load()->type == Interval::AMBIGUOUS);
        }
        t->assignIndices(1, cancel);

        auto m = Dual<3>::walk<SimplexMesher>(t, workers,
                cancel, EMPTY_PROGRESS_CALLBACK, c);

        CHECK_EDGE_PAIRS(*m);
    }

    SECTION("Sphere (higher-resolution)")
    {
        auto c = sphere(0.4);
        auto r = Region<3>({-1, -1, -1}, {1, 1, 1});

        auto t = SimplexTreePool<3>::build(c, r, 0.4, 0, 1);
        t->assignIndices(1, cancel);

        auto m = Dual<3>::walk<SimplexMesher>(t, workers,
                cancel, EMPTY_PROGRESS_CALLBACK, c);

        CHECK_EDGE_PAIRS(*m);
    }

    SECTION("Box (low-resolution)")
    {
        auto c = box({-0.4, -0.4, -0.4}, {0.4, 0.4, 0.4});
        auto r = Region<3>({-1, -1, -1}, {1, 1, 1});

        auto t = SimplexTreePool<3>::build(c, r, 0.4, 0, 1);
        t->assignIndices(1, cancel);

        auto m = Dual<3>::walk<SimplexMesher>(t, workers,
                cancel, EMPTY_PROGRESS_CALLBACK, c);

        CHECK_EDGE_PAIRS(*m);
    }
}

TEST_CASE("SimplexMesher: menger sponge")
{
    Tree sponge = max(menger(2), -sphere(1, {1.5, 1.5, 1.5}));
    Region<3> r({-2.5, -2.5, -2.5}, {2.5, 2.5, 2.5});

    auto t = SimplexTreePool<3>::build(sponge, r, 0.1);
    std::atomic_bool cancel(false);
    t->assignIndices();

    auto m = Dual<3>::walk<SimplexMesher>(t,
            8, cancel, EMPTY_PROGRESS_CALLBACK, sponge);
    REQUIRE(true);
}

TEST_CASE("SimplexTreePool: gyroid-sphere intersection vertex positions")
{
    Region<3> r({ -5, -5, -5 }, { 5, 5, 5 });

    auto s = sphereGyroid();
    auto t = SimplexTreePool<3>::build(s, r, 0.5, 1e-8, 1);

    std::list<std::pair<const SimplexTree<3>*, Region<3>>> todo;
    todo.push_back({t.get(), r});

    unsigned checked_count = 0;
    while (todo.size())
    {
        auto task = todo.front();
        todo.pop_front();

        if (task.first->isBranch())
        {
            auto rs = task.second.subdivide();
            for (unsigned i=0; i < 8; ++i) {
                todo.push_back({task.first->children[i].load(), rs[i]});
            }
        }
        else if (task.first->leaf)
        {
            for (unsigned i=0; i < 27; ++i) {
                Eigen::Vector3d vt = task.first->leaf->sub[i].load()->vert;
                CAPTURE(vt.transpose());
                CAPTURE(task.second.lower);
                CAPTURE(task.second.upper);
                REQUIRE(task.second.contains(vt));
                checked_count++;
            }
        }
    }
    REQUIRE(checked_count > 0);
}


TEST_CASE("Simplex meshing (gyroid performance breakdown)", "[!benchmark]")
{
    Region<3> r({ -5, -5, -5 }, { 5, 5, 5 });

    Root<SimplexTree<3>> t;
    unsigned workers = 8;
    std::atomic_bool cancel(false);

    auto s = sphereGyroid();

    BENCHMARK("SimplexTree construction")
    {
        t = SimplexTreePool<3>::build(s, r, 0.05, 1e-8, workers);
    }

    BENCHMARK("Assigning indices")
    {
        t->assignIndices(workers, cancel);
    }

    std::unique_ptr<Mesh> m;
    BENCHMARK("Mesh building")
    {
        m = Dual<3>::walk<SimplexMesher>(
                t, workers, cancel, EMPTY_PROGRESS_CALLBACK, s);
    }

    BENCHMARK("SimplexTree deletion")
    {
        t.reset();
    }

    BENCHMARK("Mesh deletion")
    {
        m.reset();
    }
}
