#undef UFBXT_TEST_GROUP
#define UFBXT_TEST_GROUP "audio"

UFBXT_FILE_TEST(maya_audio)
#if UFBXT_IMPL
{
	ufbx_audio_layer *layer = ufbx_as_audio_layer(ufbx_find_element(scene, UFBX_ELEMENT_AUDIO_LAYER, "audio_track1"));
	ufbxt_assert(layer);

	ufbx_audio_clip *clip = ufbx_as_audio_clip(ufbx_find_element(scene, UFBX_ELEMENT_AUDIO_CLIP, "plonk"));
	ufbxt_assert(clip);

	ufbxt_assert(layer->clips.count == 1);
	ufbxt_assert(layer->clips.data[0] == clip);

	ufbxt_assert(!strcmp(clip->relative_filename.data, "audio\\plonk.wav"));
	ufbxt_assert(!strcmp(clip->absolute_filename.data, "D:/Dev/clean/ufbx/data/audio/plonk.wav"));

	ufbxt_check_blob_content(clip->content, "audio/plonk.wav");
}
#endif

