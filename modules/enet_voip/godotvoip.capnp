@0xff75ddc6a36723c9;
struct TextMessage {
	## Enet id
	actorId @0 :UInt32;
	## Session
	sessionId @1 :UInt32;
	## Channel
	channelId @2 :UInt32;
	message @3 : Text;
}

struct Version {
	## 2-byte Major, 1-byte Minor and 1-byte Patch version number.
	versionMajor @0 :UInt32;
	versionMinor @1 :UInt8;
	versionPatch @2 :UInt8;
	## Client Release Name
	release @3 :Text;
	## OS Name
	os @4 :Text;
	osVersion @5 :Text;
}

struct UserInfo {
	userId @0 :Int32;
}

struct AudioPacket {
	header @0 :UInt8;
	sequenceNumber @1 :UInt32;
	payload @2 :Data;
}