

UFBXT_FILE_TEST(blender_279_default)
#if UFBXT_IMPL
{
	if (scene->metadata.ascii) {
		ufbxt_assert(!strcmp(scene->metadata.creator, "FBX SDK/FBX Plugins build 20070228"));
	} else {
		ufbxt_assert(!strcmp(scene->metadata.creator, "Blender (stable FBX IO) - 2.79 (sub 0) - 3.7.13"));
	}
}
#endif
