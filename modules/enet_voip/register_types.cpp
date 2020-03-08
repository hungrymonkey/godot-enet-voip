/* register_types.cpp */
#include "core/class_db.h"

#include "audio_stream_enet_voip.h"
#include "enet_voip.h"

#include "register_types.h"

void register_enet_voip_types() {
	ClassDB::register_class<AudioStreamEnetVoip>();
	ClassDB::register_class<EnetVoip>();
}

void unregister_enet_voip_types() {
}