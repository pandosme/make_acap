/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: video_object_detection.proto */

#ifndef PROTOBUF_C_video_5fobject_5fdetection_2eproto__INCLUDED
#define PROTOBUF_C_video_5fobject_5fdetection_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1003000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1003002 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _VOD__Detection VOD__Detection;
typedef struct _VOD__Scene VOD__Scene;
typedef struct _VOD__ObjectClass VOD__ObjectClass;
typedef struct _VOD__DetectorInformation VOD__DetectorInformation;


/* --- enums --- */


/* --- messages --- */

struct  _VOD__Detection
{
  ProtobufCMessage base;
  float left;
  float top;
  float right;
  float bottom;
  uint32_t id;
  uint32_t det_class;
  uint32_t score;
};
#define VOD__DETECTION__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&vod__detection__descriptor) \
    , 0, 0, 0, 0, 0, 0, 0 }


struct  _VOD__Scene
{
  ProtobufCMessage base;
  uint64_t timestamp;
  size_t n_detections;
  VOD__Detection **detections;
};
#define VOD__SCENE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&vod__scene__descriptor) \
    , 0, 0,NULL }


struct  _VOD__ObjectClass
{
  ProtobufCMessage base;
  uint32_t id;
  char *name;
};
#define VOD__OBJECT_CLASS__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&vod__object_class__descriptor) \
    , 0, (char *)protobuf_c_empty_string }


struct  _VOD__DetectorInformation
{
  ProtobufCMessage base;
  size_t n_classes;
  VOD__ObjectClass **classes;
};
#define VOD__DETECTOR_INFORMATION__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&vod__detector_information__descriptor) \
    , 0,NULL }


/* VOD__Detection methods */
void   vod__detection__init
                     (VOD__Detection         *message);
size_t vod__detection__get_packed_size
                     (const VOD__Detection   *message);
size_t vod__detection__pack
                     (const VOD__Detection   *message,
                      uint8_t             *out);
size_t vod__detection__pack_to_buffer
                     (const VOD__Detection   *message,
                      ProtobufCBuffer     *buffer);
VOD__Detection *
       vod__detection__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   vod__detection__free_unpacked
                     (VOD__Detection *message,
                      ProtobufCAllocator *allocator);
/* VOD__Scene methods */
void   vod__scene__init
                     (VOD__Scene         *message);
size_t vod__scene__get_packed_size
                     (const VOD__Scene   *message);
size_t vod__scene__pack
                     (const VOD__Scene   *message,
                      uint8_t             *out);
size_t vod__scene__pack_to_buffer
                     (const VOD__Scene   *message,
                      ProtobufCBuffer     *buffer);
VOD__Scene *
       vod__scene__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   vod__scene__free_unpacked
                     (VOD__Scene *message,
                      ProtobufCAllocator *allocator);
/* VOD__ObjectClass methods */
void   vod__object_class__init
                     (VOD__ObjectClass         *message);
size_t vod__object_class__get_packed_size
                     (const VOD__ObjectClass   *message);
size_t vod__object_class__pack
                     (const VOD__ObjectClass   *message,
                      uint8_t             *out);
size_t vod__object_class__pack_to_buffer
                     (const VOD__ObjectClass   *message,
                      ProtobufCBuffer     *buffer);
VOD__ObjectClass *
       vod__object_class__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   vod__object_class__free_unpacked
                     (VOD__ObjectClass *message,
                      ProtobufCAllocator *allocator);
/* VOD__DetectorInformation methods */
void   vod__detector_information__init
                     (VOD__DetectorInformation         *message);
size_t vod__detector_information__get_packed_size
                     (const VOD__DetectorInformation   *message);
size_t vod__detector_information__pack
                     (const VOD__DetectorInformation   *message,
                      uint8_t             *out);
size_t vod__detector_information__pack_to_buffer
                     (const VOD__DetectorInformation   *message,
                      ProtobufCBuffer     *buffer);
VOD__DetectorInformation *
       vod__detector_information__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   vod__detector_information__free_unpacked
                     (VOD__DetectorInformation *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*VOD__Detection_Closure)
                 (const VOD__Detection *message,
                  void *closure_data);
typedef void (*VOD__Scene_Closure)
                 (const VOD__Scene *message,
                  void *closure_data);
typedef void (*VOD__ObjectClass_Closure)
                 (const VOD__ObjectClass *message,
                  void *closure_data);
typedef void (*VOD__DetectorInformation_Closure)
                 (const VOD__DetectorInformation *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor vod__detection__descriptor;
extern const ProtobufCMessageDescriptor vod__scene__descriptor;
extern const ProtobufCMessageDescriptor vod__object_class__descriptor;
extern const ProtobufCMessageDescriptor vod__detector_information__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_video_5fobject_5fdetection_2eproto__INCLUDED */
