
UFBXT_CPP_TEST(test_unique_ptr)
{
    ufbx_unique_ptr<ufbx_scene> scene{ufbx_load_file(data_path("maya_cube_7500_ascii.fbx"), nullptr, nullptr)};
	ufbxt_assert(scene);
    ufbx_unique_ptr<ufbx_scene> scene2 = std::move(scene);
    ufbx_unique_ptr<ufbx_scene> scene3;
	ufbxt_assert(!scene3);
    scene3 = std::move(scene2);
}

UFBXT_CPP_TEST(test_shared_ptr)
{
    ufbx_shared_ptr<ufbx_scene> scene{ufbx_load_file(data_path("maya_cube_7500_ascii.fbx"), nullptr, nullptr)};
	ufbxt_assert(scene);
    ufbx_shared_ptr<ufbx_scene> scene2 = scene;
    ufbx_shared_ptr<ufbx_scene> scene3 = std::move(scene2);
    ufbx_shared_ptr<ufbx_scene> scene4;
	ufbxt_assert(!scene4);
    scene4 = scene3;
    scene4 = scene4;
}

UFBXT_CPP_TEST(test_unique_deleter)
{
    std::unique_ptr<ufbx_scene, ufbx_deleter> scene{ufbx_load_file(data_path("maya_cube_7500_ascii.fbx"), nullptr, nullptr)};
	ufbxt_assert(scene);
    std::unique_ptr<ufbx_scene, ufbx_deleter> scene2 = std::move(scene);
}

UFBXT_CPP_TEST(test_shared_deleter)
{
    std::shared_ptr<ufbx_scene> scene{ufbx_load_file(data_path("maya_cube_7500_ascii.fbx"), nullptr, nullptr), ufbx_deleter{}};
	ufbxt_assert(scene);
    std::shared_ptr<ufbx_scene> scene2 = scene;
    std::shared_ptr<ufbx_scene> scene3 = std::move(scene2);
}

