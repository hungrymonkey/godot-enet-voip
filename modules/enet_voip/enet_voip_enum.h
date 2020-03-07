#pragma once

enum class PacketType {
	VERSION = 0,
	TEXTMESSAGE = 1,
	AUDIOPACKET = 2,
	USERINFO = 3
};

enum class AudioCodingType {
	OPUS = 0
};