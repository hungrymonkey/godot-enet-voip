#ifndef ENET_LOCKLESS_AUDIO_STREAM_LOCKLESS_H
#define ENET_LOCKLESS_AUDIO_STREAM_LOCKLESS_H

#include "core/object.h"
#include "core/reference.h"

#include <array>
#include <atomic>
/* 
 * Single Producer, single consumer lockless ring buffer
 * only powers of two are allowed due to mask technique
 * https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/
 * https://stackoverflow.com/questions/10527581/why-must-a-ring-buffer-size-be-a-power-of-2
 */

template <uint32_t Q_SIZE>
class VoipAudioStreamBuffer : public Reference {
	GDCLASS(VoipAudioStreamBuffer, Object);

	const uint32_t Q_MASK = Q_SIZE - 1;

private:
	std::atomic<uint32_t> _head;
	//std::atomic<uint64_t> _head_last;
	std::atomic<uint32_t> _tail;
	//I shouldnt have to turn each byte into atomic
	//variable
	//TODO fix this. atomic are slow primatives
	std::array<std::atomic<uint8_t>, Q_SIZE> _buffer = {};

public:
	bool put_byte_array(const uint8_t *data, int32_t size) {
		uint32_t head = _head.load();
		uint32_t tail = _tail.load();

		uint32_t available_size = Q_SIZE - (head - tail);
		uint32_t smaller = MIN((uint32_t)size, available_size);

		for (uint32_t i = 0; i < smaller; i++) {
			_buffer[(head + i) & Q_MASK].store(data[i]);
		}
		_head.store((head + smaller));
		return size == (int32_t)smaller;
	}

	int32_t get_byte_array(uint8_t *data, int32_t size) {
		uint32_t head = _head.load();
		uint32_t tail = _tail.load();
		uint32_t available_size = head - tail;

		uint32_t smaller = MIN((uint32_t)size, available_size);
		for (uint32_t i = 0; i < smaller; i++) {
			data[i] = _buffer[(tail + i) & Q_MASK].load();
		}
		_tail.store((tail + smaller));
		return (int32_t)smaller;
	}
	void clear() {

		_head.store(0xffffffff - 4000);
		_tail.store(0xffffffff - 4000);
	}
	bool is_empty() const {
		return _head.load() == _tail.load();
	}
	int32_t size() const {
		uint32_t head = _head.load();
		uint32_t tail = _tail.load();
		int32_t ret = (int32_t)(head - tail);
		return ret;
	}
	VoipAudioStreamBuffer() {
		//fuzzing output i guess
		_head.store(0xffffffff - 4000);
		//_head_last.store(0);
		_tail.store(0xffffffff - 4000);
	};
};
#endif