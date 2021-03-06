/*
  ==============================================================================
   
   Copyright (c) 2014 Jacob Sologub
   
   Permission is hereby granted, free of charge, to any person obtaining a copy 
   of this software and associated documentation files (the "Software"), to deal 
   in the Software without restriction, including without limitation the rights 
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
   copies of the Software, and to permit persons to whom the Software is 
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in 
   all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
   SOFTWARE.

  ==============================================================================
*/

#include <Magick++.h>
#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <string.h>

using namespace std;
using namespace v8;
using namespace node;

Handle<Value> convert (const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 2) {
        return ThrowException (Exception::TypeError (String::New ("Wrong number of arguments.")));
    }

    if (!args [0]->IsObject()) {
        return ThrowException (Exception::TypeError (String::New ("First argument must be be a buffer.")));
    }

    if (!args [1]->IsFunction()) {
        return ThrowException (Exception::TypeError (String::New ("Second argument must be a callback function.")));
    }

    Local<Object> buffer = args [0]->ToObject();
    const size_t size = Buffer::Length (buffer);
  	const char* data = Buffer::Data (buffer);

  	Magick::Blob blob (data, size);
  	list <Magick::Image> imageList;
    Magick::readImages (&imageList, blob);
    Handle<Array> bufferArray = v8::Array::New (imageList.size());
    Local<Object> globalObj = v8::Context::GetCurrent()->Global();

    for (list <Magick::Image>::iterator it = imageList.begin(); it != imageList.end(); ++it) {
      	Magick::Blob blob;
      	it->write (&blob, "jpeg");

      	Buffer* slowBuffer = node::Buffer::New (blob.length());
      	memcpy (node::Buffer::Data (slowBuffer), blob.data(), blob.length());
      	Local<Function> bufferConstructor = Local<Function>::Cast (globalObj->Get (String::New ("Buffer")));

      	Handle<Value> constructorArgs [3] = { 
            slowBuffer->handle_, 
        		Integer::New (blob.length()), 
        		Integer::New (0) 
      	};

      	const int index = distance (imageList.begin(), it);
      	Local<Object> actualBuffer = bufferConstructor->NewInstance (3, constructorArgs);
      	bufferArray->Set (index, actualBuffer);
    }

    const unsigned argc = 2;
    Local<Value> argv [argc] = {
        Local<Value>::New (Null()),
        Local<Value>::New (bufferArray)
    };

    Local<Function> callback = Local<Function>::Cast (args [1]);
    callback->Call (Context::GetCurrent()->Global(), argc, argv);

	return Undefined();
}

Handle<Value> getType (const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1) {
        return ThrowException (Exception::TypeError (String::New ("Wrong number of arguments.")));
    }

    if (!args [0]->IsObject()) {
        return ThrowException (Exception::TypeError (String::New ("First argument must be be a buffer.")));
    }

    Local<Object> buffer = args [0]->ToObject();
    const size_t size = Buffer::Length (buffer);
    const char* data = Buffer::Data (buffer);

    Magick::Blob blob (data, size);
    Magick::Image image;
    image.read (blob);
    string type = image.magick().c_str();

    const unsigned argc = 2;
    Local<Value> argv [argc] = {
        Local<Value>::New (Null()),
        Local<Value>::New (String::New (type.c_str()))
    };

    Local<Function> callback = Local<Function>::Cast (args [1]);
    callback->Call (Context::GetCurrent()->Global(), argc, argv);

    return Undefined();
}

Handle<Value> getSize (const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 1) {
        return ThrowException (Exception::TypeError (String::New ("Wrong number of arguments.")));
    }

    if (!args [0]->IsObject()) {
        return ThrowException (Exception::TypeError (String::New ("First argument must be be a buffer.")));
    }

    Local<Object> buffer = args [0]->ToObject();
    const size_t size = Buffer::Length (buffer);
    const char* data = Buffer::Data (buffer);

    Magick::Blob blob (data, size);
    Magick::Image image;
    image.read (blob);
    Magick::Geometry geometry = image.size();

    Local<Object> result = Object::New();
    result->Set (String::NewSymbol ("w"), Number::New (geometry.width()));
    result->Set (String::NewSymbol ("h"), Number::New (geometry.height()));

    const unsigned argc = 2;
    Local<Value> argv [argc] = {
        Local<Value>::New (Null()),
        Local<Value>::New (result)
    };

    Local<Function> callback = Local<Function>::Cast (args [1]);
    callback->Call (Context::GetCurrent()->Global(), argc, argv);

    return Undefined();
}

Handle<Value> extract (const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 2) {
        return ThrowException (Exception::TypeError (String::New ("Wrong number of arguments.")));
    }

    if (!args [0]->IsObject()) {
        return ThrowException (Exception::TypeError (String::New ("First argument must be be an object.")));
    }

    if (!args [1]->IsFunction()) {
        return ThrowException (Exception::TypeError (String::New ("Second argument must be a callback function.")));
    }

    Local<Object> options = args [0]->ToObject();

    Local<Object> buffer = Local<Object>::Cast (options->Get (String::NewSymbol ("source")));
    if (buffer->IsUndefined() || !Buffer::HasInstance (buffer)) {
        return ThrowException (Exception::TypeError (String::New ("First argument should have \"source\" key with a Buffer instance")));
    }

    Local<Array> regions = Local<Array>::Cast (options->Get (String::NewSymbol ("regions")));
    if (regions->IsUndefined()) {
        return ThrowException (Exception::TypeError (String::New ("First argument should have \"regions\" key with an Array of rectangles \"{ x: 0, y: 0, w: 100, h: 50 }\"")));
    }

    const size_t size = Buffer::Length (buffer);
    const char* data = Buffer::Data (buffer);
    Magick::Blob blob (data, size);

    if (blob.length() <= 0) {
        const unsigned argc = 2;
        Local<Value> argv [argc] = {
            Local<Value>::New (Null()),
            Local<Value>::New (Null())
        };

        Local<Function> callback = Local<Function>::Cast (args [1]);
        callback->Call (Context::GetCurrent()->Global(), argc, argv);

        return Undefined();
    }

    Handle<Array> bufferArray = v8::Array::New ();
    Local<Object> globalObj = v8::Context::GetCurrent()->Global();

    for (int i = 0; i < (int) regions->Length(); ++i) {
        Local<Object> r = Local<Object>::Cast (regions->Get (i));
        Local<Number> x = Local<Number>::Cast (r->Get (String::NewSymbol ("x")));
        Local<Number> y = Local<Number>::Cast (r->Get (String::NewSymbol ("y")));
        Local<Number> w = Local<Number>::Cast (r->Get (String::NewSymbol ("w")));
        Local<Number> h = Local<Number>::Cast (r->Get (String::NewSymbol ("h")));

        if (x->IsNumber() && y->IsNumber() && w->IsNumber() && h->IsNumber()) {
            Magick::Image image;
            Magick::Blob cropBlob;

            image.read (blob);
            Magick::Geometry originalGeometry = image.size();
            Magick::Geometry geometry (w->Value(), h->Value(), x->Value(), y->Value());

            if ((int) (originalGeometry.width() - geometry.xOff()) < 1 || (int) (originalGeometry.height() - geometry.yOff()) < 1) {
                continue;
            }

            image.crop (geometry);
            image.write (&cropBlob, "jpeg");

            Buffer* slowBuffer = node::Buffer::New (cropBlob.length());
            memcpy (node::Buffer::Data (slowBuffer), cropBlob.data(), cropBlob.length());
            Local<Function> bufferConstructor = Local<Function>::Cast (globalObj->Get (String::New ("Buffer")));

            Handle<Value> constructorArgs [3] = { 
                slowBuffer->handle_, 
                Integer::New (cropBlob.length()), 
                Integer::New (0) 
            };

            Local<Object> actualBuffer = bufferConstructor->NewInstance (3, constructorArgs);
            bufferArray->Set (i, actualBuffer);
        }
    }

    const unsigned argc = 2;
    Local<Value> argv [argc] = {
        Local<Value>::New (Null()),
        Local<Value>::New (bufferArray)
    };

    Local<Function> callback = Local<Function>::Cast (args [1]);
    callback->Call (Context::GetCurrent()->Global(), argc, argv);

    return Undefined();
}

Handle<Value> animate (const Arguments& args) {
    HandleScope scope;

    if (args.Length() < 2) {
        return ThrowException (Exception::TypeError (String::New ("Wrong number of arguments.")));
    }

    if (!args [0]->IsObject()) {
        return ThrowException (Exception::TypeError (String::New ("First argument must be be an object.")));
    }

    if (!args [1]->IsFunction()) {
        return ThrowException (Exception::TypeError (String::New ("Second argument must be a callback function.")));
    }

    Local<Object> options = args [0]->ToObject();

    Local<Array> buffers = Local<Array>::Cast (options->Get (String::NewSymbol ("buffers")));
    if (buffers->IsUndefined()) {
        return ThrowException (Exception::TypeError (String::New ("First argument should have \"buffers\" key with an Array of buffers \"{ x: 0, y: 0, w: 100, h: 50 }\"")));
    }

    Local<Number> delay = Local<Number>::Cast (options->Get (String::NewSymbol ("delay")));

    vector<Magick::Image> frames;
    for (int i = 0; i < (int) buffers->Length(); ++i) {
        Local<Object> buffer = Local<Object>::Cast (buffers->Get (i));
        if (buffer->IsUndefined() || !Buffer::HasInstance (buffer)) {
            continue;
        }

        const size_t size = Buffer::Length (buffer);
        const char* data = Buffer::Data (buffer);
        Magick::Blob b (data, size);

        if (b.length() > 0) {
            Magick::Image image;
            image.read (b);
            image.magick ("gif");

            if (delay->IsNumber()) {
                image.animationDelay (delay->Value());  
            }
            else {
                image.animationDelay (buffers->Length() * 0.075f);
            }

            frames.push_back (image);
        }
    }

    Magick::Blob blob;
    Magick::writeImages (frames.begin(), frames.end(), &blob);

    Buffer* slowBuffer = node::Buffer::New (blob.length());
    memcpy (node::Buffer::Data (slowBuffer), blob.data(), blob.length());
    Local<Object> globalObj = v8::Context::GetCurrent()->Global();
    Local<Function> bufferConstructor = Local<Function>::Cast (globalObj->Get (String::New ("Buffer")));

    Handle<Value> constructorArgs [3] = { 
        slowBuffer->handle_, 
        Integer::New (blob.length()), 
        Integer::New (0) 
    };

    Local<Object> actualBuffer = bufferConstructor->NewInstance (3, constructorArgs);

    const unsigned argc = 2;
    Local<Value> argv [argc] = {
        Local<Value>::New (Null()),
        Local<Value>::New (actualBuffer)
    };

    Local<Function> callback = Local<Function>::Cast (args [1]);
    callback->Call (Context::GetCurrent()->Global(), argc, argv);

    return Undefined();
}

Handle<Value> composite (const Arguments& args) {
    if (args.Length() < 2) {
        return ThrowException (Exception::TypeError (String::New ("Wrong number of arguments.")));
    }

    if (!args [0]->IsObject()) {
        return ThrowException (Exception::TypeError (String::New ("First argument must be be an object.")));
    }

    if (!args [1]->IsFunction()) {
        return ThrowException (Exception::TypeError (String::New ("Second argument must be a callback function.")));
    }

    Local<Object> options = args [0]->ToObject();

    Local<Object> buffer = Local<Object>::Cast (options->Get (String::NewSymbol ("buffer")));
    Local<Object> overlayBuffer = Local<Object>::Cast (options->Get (String::NewSymbol ("overlayBuffer")));

    if (buffer->IsUndefined()) {
        return ThrowException (Exception::TypeError (String::New ("First argument should have a \"buffer\" key with an image buffer")));
    }

    if (overlayBuffer->IsUndefined()) {
        return ThrowException (Exception::TypeError (String::New ("First argument should have a \"overlayBuffer\" key with an image buffer")));
    }

    Local<Number> compositeOperator = Local<Number>::Cast (options->Get (String::NewSymbol ("operator")));
    (void) compositeOperator;

    const size_t imageSize = Buffer::Length (buffer);
    const char* imageData = Buffer::Data (buffer);

    Magick::Blob iamageBlob (imageData, imageSize);
    Magick::Image image;
    image.read (iamageBlob);

    const size_t compositeImagesize = Buffer::Length (overlayBuffer);
    const char* compositeImageData = Buffer::Data (overlayBuffer);

    Magick::Blob overlayBlob (compositeImageData, compositeImagesize);
    Magick::Image overlayImage;
    overlayImage.read (overlayBlob);

    Magick::Image finalImage;
    finalImage.size (image.size());
    finalImage.matte (true);

    Magick::CompositeOperator op = Magick::CompositeOperator::AtopCompositeOp;
    finalImage.composite (image, 0, 0, op);
    finalImage.composite (overlayImage, 0, 0, op);

    Magick::Blob compositedBlob;
    finalImage.write (&compositedBlob, "jpeg");

    Buffer* slowBuffer = node::Buffer::New (compositedBlob.length());
    memcpy (node::Buffer::Data (slowBuffer), compositedBlob.data(), compositedBlob.length());
    Local<Object> globalObj = v8::Context::GetCurrent()->Global();
    Local<Function> bufferConstructor = Local<Function>::Cast (globalObj->Get (String::New ("Buffer")));

    Handle<Value> constructorArgs [3] = { 
        slowBuffer->handle_, 
        Integer::New (compositedBlob.length()), 
        Integer::New (0) 
    };

    Local<Object> actualBuffer = bufferConstructor->NewInstance (3, constructorArgs);

    const unsigned argc = 2;
    Local<Value> argv [argc] = {
        Local<Value>::New (Null()),
        Local<Value>::New (actualBuffer)
    };

    Local<Function> callback = Local<Function>::Cast (args [1]);
    callback->Call (Context::GetCurrent()->Global(), argc, argv);

    return Undefined();
}

void RegisterModule (v8::Handle<v8::Object> target) {
     target->Set (String::NewSymbol ("convert"), FunctionTemplate::New (convert)->GetFunction());
     target->Set (String::NewSymbol ("getType"), FunctionTemplate::New (getType)->GetFunction());
     target->Set (String::NewSymbol ("getSize"), FunctionTemplate::New (getSize)->GetFunction());
     target->Set (String::NewSymbol ("extract"), FunctionTemplate::New (extract)->GetFunction());
     target->Set (String::NewSymbol ("animate"), FunctionTemplate::New (animate)->GetFunction());
     target->Set (String::NewSymbol ("composite"), FunctionTemplate::New (composite)->GetFunction());
}

NODE_MODULE (gif2jpg, RegisterModule);
