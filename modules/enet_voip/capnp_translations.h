#include <capnp/serialize-packed.h>

#include <core/vector.h>

class Utils {
public:
	static size_t messageByteSize(::capnp::MessageBuilder &builder) {
		auto segments = builder.getSegmentsForOutput();
		size_t sz = sizeof(uint32_t) * ((segments.size() + 2) & ~size_t(1));
		// size of table
		for (auto segment : segments) {
			sz += segment.asBytes().size();
		}
		return sz;
	}
};

class GVectorBuffer : public kj::BufferedOutputStream {

public:
	GVectorBuffer(Vector<uint8_t> &vec) {
		_vectorBuffer = &vec;
	}
	size_t available() {
		return _vectorBuffer->size() - bytesWritten;
	}
	/*
		 * kj::ArrayPtr<kj::byte> contains both ptr and buffer size
		 * https://github.com/capnproto/capnproto/blob/master/c++/src/kj/common.h#L1388
		 * 
		 */
	kj::ArrayPtr<kj::byte> getWriteBuffer() override {
		uint8_t *wptr = _vectorBuffer->ptrw();
		return kj::ArrayPtr<kj::byte>(&wptr[bytesWritten], available());
	}

	void write(const void *src, size_t size) override {
		if (bytesWritten + size > _vectorBuffer->size()) {
			resize(bytesWritten + size);
		}
		uint8_t *wptr = _vectorBuffer->ptrw();
		memcpy(wptr + bytesWritten, src, size);
		bytesWritten += size;
	}
	Error resize(int p_size) { return _vectorBuffer->resize(p_size); }
	size_t size() {
		return bytesWritten;
	}

private:
	Vector<uint8_t> *_vectorBuffer;
	size_t bytesWritten;
};