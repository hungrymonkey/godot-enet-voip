/* register_types.cpp */

#include "register_types.h"
#include "audio_stream_enet_voip.h"
#include "core/class_db.h"

void register_enet_voip_types() {
	ClassDB::register_class<AudioStreamEnetVoip>();
}

void unregister_enet_voip_types() {
}