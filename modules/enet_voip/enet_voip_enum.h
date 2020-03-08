#pragma once

enum class PacketType {
	AUDIOPACKET = 0,
	TEXTMESSAGE = 1,
	VERSION = 2,
	USERINFO = 3
};

enum class AudioCodingType {
	OPUS = 0
};