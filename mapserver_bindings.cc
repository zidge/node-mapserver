/*
Copyright (c) 2010, Paul Spencer <pagameba@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <mapserver.h>
#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <string.h>
#include <assert.h>

using namespace v8;
using namespace node;

#define THROW_ERROR(TYPE, STR)                                          \
  return ThrowException(Exception::TYPE(String::New(STR)));

#define REQ_ARGS(N)                                                     \
  if (args.Length() < (N))                                              \
    return ThrowException(Exception::TypeError(                         \
                             String::New("Expected " #N "arguments"))); 

#define REQ_STR_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsString())                     \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a string")));    \
  String::Utf8Value VAR(args[I]->ToString());

#define REQ_INT_ARG(I, VAR)                                             \
  int VAR;                                                              \
  if (args.Length() <= (I) || !args[I]->IsInt32())                      \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be an integer")));  \
  VAR = args[I]->Int32Value();

#define REQ_DOUBLE_ARG(I, VAR)                                          \
  double VAR;                                                           \
  if (args.Length() <= (I) || !args[I]->IsNumber())                     \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a number")));    \
  VAR = args[I]->NumberValue();

#define REQ_EXT_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsExternal())                   \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " invalid")));             \
  Local<External> VAR = Local<External>::Cast(args[I]);

#define OPT_INT_ARG(I, VAR, DEFAULT)                                    \
  int VAR;                                                              \
  if (args.Length() <= (I)) {                                           \
    VAR = (DEFAULT);                                                    \
  } else if (args[I]->IsInt32()) {                                      \
    VAR = args[I]->Int32Value();                                        \
  } else {                                                              \
    return ThrowException(Exception::TypeError(                         \
              String::New("Argument " #I " must be an integer"))); \
  }

#define RETURN_LAYER(LAYER)                                               \
  if (!LAYER) return Null();                                            \
  HandleScope scope;                                                  \
  Local<Value> _arg_ = External::New(LAYER);                            \
  Persistent<Object> _layer_(Layer::constructor_template->GetFunction()->NewInstance(1, &_arg_)); \
  return scope.Close(_layer_);

#define RETURN_MAP(MAP)                                               \
  if (!MAP) return Null();                                            \
  HandleScope scope;                                                  \
  Local<Value> _arg_ = External::New(MAP);                            \
  Persistent<Object> _map_(Map::constructor_template->GetFunction()->NewInstance(1, &_arg_)); \
  return scope.Close(_map_);

#define RETURN_ERROR(ERROR)                                               \
  if (!ERROR) return Null();                                            \
  HandleScope scope;                                                  \
  Local<Value> _arg_ = External::New(ERROR);                            \
  Persistent<Object> _err_(ErrorObj::constructor_template->GetFunction()->NewInstance(1, &_arg_)); \
  return scope.Close(_err_);

#define RETURN_STRING(STR)                                              \
  HandleScope scope;                                                    \
  Local<String> result = String::New(STR);                              \
  return scope.Close(result);

#define RETURN_NUMBER(NUM)                                                   \
  HandleScope scope;                                                    \
  Local<Number> result = Integer::New(NUM);                             \
  return scope.Close(result);

class Mapserver {

  public:
    static void Init(Handle<Object> target) 
    {
      HandleScope scope;
      
      NODE_SET_METHOD(target, "loadMap", LoadMap);
      NODE_SET_METHOD(target, "getVersion", GetVersion);
      NODE_SET_METHOD(target, "getVersionInt", GetVersionInt);

      NODE_SET_METHOD(target, "resetErrorList", ResetErrorList);
      NODE_SET_METHOD(target, "getError", GetError);
      
      ErrorObj::Init(target);
      Map::Init(target);
      Layer::Init(target);
    }

  protected:
    
    static Handle<Value> LoadMap (const Arguments& args) {
      REQ_STR_ARG(0, filename);
      REQ_STR_ARG(1, path);
      
      mapObj * map = msLoadMap(*filename, *path);
      if (map == NULL) {
        THROW_ERROR(Error, "Ugh");
      }
      RETURN_MAP(map);
    }
    
    static Handle<Value> ResetErrorList (const Arguments& args) {
      msResetErrorList();
      return Undefined();
    }
    
    static Handle<Value> GetError (const Arguments& args) {
      errorObj * err = msGetErrorObj();
      
      if (err == NULL) {
        return Undefined();
      } 
      RETURN_ERROR(err);
    }
    
    static Handle<Value> GetVersion (const Arguments& args) {
      HandleScope scope;
      char * version = msGetVersion();

      Local<String> result = String::New(version);
      return scope.Close(result);
    }
    
    static Handle<Value> GetVersionInt (const Arguments& args) {
      HandleScope scope;
      int version = msGetVersionInt();

      Local<Number> result = Integer::New(version);
      return scope.Close(result);
    }
    
    
    class ErrorObj : public ObjectWrap {
    public:
      static Persistent<FunctionTemplate> constructor_template;
      
      static void Init(v8::Handle<Object> target) {
        HandleScope scope;
        
        Local<FunctionTemplate> t = FunctionTemplate::New(New);
        constructor_template = Persistent<FunctionTemplate>::New(t);
        
        t->InstanceTemplate()->SetInternalFieldCount(1);
        t->PrototypeTemplate()->SetNamedPropertyHandler(NamedPropertyGetter);
      
        target->Set(String::NewSymbol("ErrorObj"), t->GetFunction());
      }
      
      static Handle<Value> New(const Arguments& args) {
        HandleScope scope;
        REQ_EXT_ARG(0, err);
        (new ErrorObj((errorObj *)err->Value()))->Wrap(args.This());
        return args.This();
      }
    protected:
      ErrorObj(errorObj *err) : _err(err) { }
      
      ~ErrorObj() { }
      
      errorObj * _err;
      
      operator errorObj* () const { return _err; }
      
      static Handle<Value> NamedPropertyGetter (Local<String> property, const AccessorInfo& info) {
        ErrorObj *err = ObjectWrap::Unwrap<ErrorObj>(info.This());
        v8::String::AsciiValue n(property);
        if (strcmp(*n, "code") == 0) {
          RETURN_NUMBER(err->_err->code);
        } else if (strcmp(*n, "codeStr") == 0) {
          RETURN_STRING(msGetErrorCodeString(err->_err->code));
        } else if (strcmp(*n, "message") == 0) {
          RETURN_STRING(err->_err->message);
        } else if (strcmp(*n, "routine") == 0) {
          RETURN_STRING(err->_err->routine);
        }
        return Undefined();
      }
    };
  
  
    class Map : public ObjectWrap {
      public:
        static Persistent<FunctionTemplate> constructor_template;
      
        static void Init(v8::Handle<Object> target) {
          HandleScope scope;
        
          Local<FunctionTemplate> t = FunctionTemplate::New(New);
          constructor_template = Persistent<FunctionTemplate>::New(t);
        
          t->InstanceTemplate()->SetInternalFieldCount(1);
        
          NODE_SET_PROTOTYPE_METHOD(t, "drawMap", DrawMap);
        
          t->InstanceTemplate()->SetNamedPropertyHandler(NamedPropertyGetter, NamedPropertySetter);
        
          target->Set(String::NewSymbol("Map"), t->GetFunction());
        }
      
        static Handle<Value> New(const Arguments& args) {
          assert(args.IsConstructCall());
          HandleScope scope;
          REQ_EXT_ARG(0, map);
          (new Map((mapObj *)map->Value()))->Wrap(args.This());
          return args.This();
        }

      protected:  
        Map(mapObj * map) : _map(map) { 
        }

        ~Map() { 
          if (_map) {
            msFreeMap(_map);
          }
        }

        mapObj * _map;
        
        operator mapObj* () const { return _map; }
        
        static Persistent<ObjectTemplate> layers_template_;
        
        static Handle<Value> Destroy (const Arguments &args) {
          Map *map = ObjectWrap::Unwrap<Map>(args.This());
          msFreeMap(map->_map);
          return Undefined();
        }

        static Handle<Value> NamedPropertyGetter (Local<String> property, const AccessorInfo& info) {
          Map *map = ObjectWrap::Unwrap<Map>(info.This());
          v8::String::AsciiValue n(property);
          if (strcmp(*n, "width") == 0) {
            RETURN_NUMBER(map->_map->width);
          } else if (strcmp(*n, "height") == 0) {
            RETURN_NUMBER(map->_map->height);
          } else if (strcmp(*n, "layers") == 0) {
            if (layers_template_.IsEmpty()) {
              Handle<ObjectTemplate> raw_template = ObjectTemplate::New();
              raw_template->SetInternalFieldCount(1);
              raw_template->SetIndexedPropertyHandler(LayersIndexedGetter, NULL, NULL, NULL, NULL);
              raw_template->SetNamedPropertyHandler(LayersNamedGetter, NULL, NULL, NULL, NULL);
              layers_template_ = Persistent<ObjectTemplate>::New(raw_template);
            }
            Handle<ObjectTemplate> templ = layers_template_;
            Handle<Object> result = templ->NewInstance();
            Handle<External> map_ptr = External::New(map);
            result->SetInternalField(0,map_ptr);
            HandleScope scope;
            return scope.Close(result);
          }
          return Undefined();
        }
        
        static Handle<Value> NamedPropertySetter (Local<String> property, Local<Value> value, const AccessorInfo& info) {
          Map *map = ObjectWrap::Unwrap<Map>(info.Holder());
          v8::String::AsciiValue n(property);
          if (strcmp(*n, "width") == 0) {
            map->_map->width = value->Int32Value();
          } else if (strcmp(*n, "height") == 0) {
            map->_map->height = value->Int32Value();
          }
        }
                
        static Handle<Value> LayersIndexedGetter (uint32_t index, const AccessorInfo& info) {
          Map *map = ObjectWrap::Unwrap<Map>(info.This());
          if (index >=0 && index < map->_map->numlayers) {
            RETURN_LAYER(map->_map->layers[index]);
          }
          return Undefined();
        }
        
        static Handle<Value> LayersNamedGetter(Local<String> name, const AccessorInfo& info) {
          Map *map = ObjectWrap::Unwrap<Map>(info.This());
          String::AsciiValue n(name);
          if (strcmp(*n, "length") == 0) {
            return Integer::New(map->_map->numlayers);
          }
          return Undefined();
        }
        
        
        /**
         * callback for buffer creation to free the memory assocatiated with the
         * image after its been copied into the buffer
         */
        static void FreeImageBuffer(char *data, void *hint) {
          msFree(data);
        }
  
        static Handle<Value> DrawMap (const Arguments& args) {
          HandleScope scope;
          Map *map = ObjectWrap::Unwrap<Map>(args.This());
          imageObj * im = msDrawMap(map->_map, MS_FALSE);
          int size;

          unsigned char * data = msSaveImageBuffer(im, &size, map->_map->outputformat);
          Buffer *retbuf = Buffer::New((char *) data, size, FreeImageBuffer, NULL);
          msFreeImage(im);

          return scope.Close(retbuf->handle_);
        }
    };
    
    
    class Layer : public ObjectWrap {
      public:
        static Persistent<FunctionTemplate> constructor_template;
      
        static void Init(v8::Handle<Object> target) {
          HandleScope scope;
        
          Local<FunctionTemplate> t = FunctionTemplate::New(New);
          constructor_template = Persistent<FunctionTemplate>::New(t);
        
          t->InstanceTemplate()->SetInternalFieldCount(1);
        
          // NODE_SET_PROTOTYPE_METHOD(t, "drawMap", DrawMap);
        
          t->InstanceTemplate()->SetNamedPropertyHandler(NamedPropertyGetter, NamedPropertySetter);
        
          target->Set(String::NewSymbol("Layer"), t->GetFunction());
        }
      
        static Handle<Value> New(const Arguments& args) {
          HandleScope scope;
          REQ_EXT_ARG(0, layer);
          (new Layer((layerObj *)layer->Value()))->Wrap(args.This());
          return args.This();
        }

      protected:  
        Layer(layerObj * layer) : _layer(layer) { }

        ~Layer() { 
          if (_layer) {
            // msFreeLayer(_layer);
          }
        }

        layerObj * _layer;

        operator layerObj* () const { return _layer; }
        
        static Handle<Value> Destroy (const Arguments &args) {
          Layer *layer = ObjectWrap::Unwrap<Layer>(args.This());
          // msFreeLayer(layer->_layer);
          return Undefined();
        }
        
        static Handle<Value> NamedPropertyGetter (Local<String> property, const AccessorInfo& info) {
          Layer *layer = ObjectWrap::Unwrap<Layer>(info.This());
          v8::String::AsciiValue n(property);
          if (strcmp(*n, "name") == 0) {
            RETURN_STRING(layer->_layer->name);
          }
          return Undefined();
        }
        
        static Handle<Value> NamedPropertySetter (Local<String> property, Local<Value> value, const AccessorInfo& info) {
          Layer *layer = ObjectWrap::Unwrap<Layer>(info.Holder());
          v8::String::AsciiValue n(property);
          if (strcmp(*n, "name") == 0) {
            String::Utf8Value name(value->ToString());
            layer->_layer->name = *name;
          }
        }
      };
};

Persistent<FunctionTemplate> Mapserver::ErrorObj::constructor_template;
Persistent<FunctionTemplate> Mapserver::Map::constructor_template;
Persistent<ObjectTemplate>   Mapserver::Map::layers_template_;
Persistent<FunctionTemplate> Mapserver::Layer::constructor_template;

extern "C"
void init (Handle<Object> target)
{
  HandleScope scope;
  Mapserver::Init(target);
}
