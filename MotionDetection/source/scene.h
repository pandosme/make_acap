/**
 *  @file   scene.h
 *  @author Johan Almbladh
 *  @brief  Scene description API.
 *
 *  Copyright (C) 2006-2007, Axis Communications AB, LUND, SWEDEN
 *
 *  @mainpage
 *  @section  Scene Description API
 *  @subsection contents Table of Contents
 *  - @ref scene_create     "Scene instance administration"
 *  - @ref scene_serialize  "XML serialization"
 *  - @ref scene_add_object "Scene object management"
 *  - @ref scene_add_event  "Scene event management"
 *  - @ref scene_get_prop   "Scene/object/event property access"
 *  - @ref scene_prop_e     "List of properties"
 */

#ifndef SCENE_H
#define SCENE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  OK return value.
 */
#define SCENE_OK     0

/**
 *  Failure return value.
 */
#define SCENE_FAIL (-1)

/*
 * -------------------------------------------------------------
 *  Type definitions
 * -------------------------------------------------------------
 */

/**
 *  The scene and object properties.
 */
typedef enum scene_prop_e {
    /**
     *  Time stamp, integral uint64_t scalar.
     */
    SCENE_PROP_TIME,

    /**
     *  Camera pan angle in degrees, fixed-point int scalar.
     */
    SCENE_PROP_PAN,

    /**
     *  Camera tilt angle in degrees, fixed-point int scalar.
     */
    SCENE_PROP_TILT,

    /**
     *  Camera zoom factor, fixed-point int scalar.
     */
    SCENE_PROP_ZOOM,

    /**
     *  An extra id for objects to be used for associating
     *  objects with each other.
     */
    SCENE_PROP_GROUP_ID,

    /**
     *  Object bounding box, fixed-point int16_t vector of length 4.
     *  The vector elements are {xmin, ymin, width, height}.
     */
    SCENE_PROP_BOX,

    /**
     *  Object polygon, fixed-point int16_t vector of 2D vectors.
     *  The vector elements are vertices {x, y}. The length argument
     *  is the number of vertices in the polygon.
     */
    SCENE_PROP_POLYGON,

    /**
     *  Object velocity, fixed-point int vector of length 2.
     *  The vector elements are the velocity components {vx, vy}.
     */
    SCENE_PROP_VELOCITY,

    /**
     *  Object color, fixed-point int vector of 7D color decriptors.
     *  The color descriptor components are {y, cb, cr, y_stdev,
     *  cb_stdev, cr_stdev, coverage}. The length argument is the
     *  number of color descriptors.
     */
    SCENE_PROP_COLOR,

    /**
     *  Event object ID, integral int scalar.
     */
    SCENE_PROP_REF,

    /**
     *  Event object IDs, integral int vector of arbitrary length.
     *  The length argument is the number of IDs in the vector.
     */
    SCENE_PROP_REFS,
    SCENE_PROP_ID,
    SCENE_PROP_FLAGS,
    SCENE_PROP_FRAC,
    SCENE_PROP_ACTION,

    /**
     *  Object confidence value, fixed-point int scalar.
     */
    SCENE_PROP_CONFIDENCE,

    /**
     *  Object velocity, fixed-point int vector of length 3.
     *  The vector elements are {speed, azimuth, elevation}.
     */
    SCENE_PROP_3D_VELOCITY,

    /**
     * Depth position, fixed-point int vector of length 3.
     * The vector elements are {distance, azimuth, elevation}.
     */
    SCENE_PROP_DEPTH_POSITION,

    /**
     * Object classification, integral int scalar.
     */
    SCENE_PROP_CLASS,

    /**
     * Object Area, fixed-point int scalar.
     */
    SCENE_PROP_AREA,

    SCENE_NUM_PROPS
} scene_prop_t;

/**
 *  The event actions.
 */
typedef enum scene_action_t {
    /**
     *  An object is deleted from the scene.
     */
    SCENE_ACT_DELETE,

     /**
      *  An object is split into two or more new objects.
      */
    SCENE_ACT_SPLIT,

     /**
      *  Two or more objects are merged into a new object.
      */
    SCENE_ACT_MERGE,
    SCENE_NUM_ACTIONS
} scene_action_t;

/**
 *  The opaque scene instance data type.
 */
typedef struct scene_st scene_t;

/**
 *  The scene object data type.
 */
typedef struct scene_object_st scene_object_t;

/**
 *  The scene event data type.
 */
typedef struct scene_event_st scene_event_t;


/*
 * -------------------------------------------------------------
 *  Scene global functions
 * -------------------------------------------------------------
 */

/**
 *  Create a new scene.
 *  @param  frac  The number of fractional bits to use in the
 *                fixed-point representation.
 *  @return       A new scene object.
 */
scene_t*
scene_create(unsigned frac);

/**
 *  Delete a scene.
 *  @param  scene   The scene object to delete.
 */
void
scene_delete(scene_t *scene);

/**
 *  Ref a scene
 *  @param  scene   The scene object to ref.
 */
scene_t*
scene_ref(scene_t *scene);

/**
 *  Unref a scene
 *  @param  scene   The scene object to unref.
 */
void
scene_unref(scene_t *scene);

/**
 *  Deepcopy a scene object.
 *  @param  scene The scene to copy.
 *  @return       The copied scene.
 */
scene_t*
scene_copy(const scene_t *scene);

/**
 *  Deepcopy a scene object in a serialized manner to the area which is pointed
 *  to by buf. Buf must be big enough to handle the whole scene.
 *  @param  scene        The scene to copy.
 *  @param  buf          The buffer to copy buf to.
 *  @param  bounding_box Include bounding_box data.
 *  @param  polygon      Include polygon data.
 *  @param  velocity     Include velocity data.
 *  @return              Returns SCENE_FAIL when at least one of the objects in
 *                       the scene do not adhere to the requested bounding_box,
 *                       polygon and/or velocity settings, i.e. the caller of
 *                       this function expects e.g. bounding box but one of the
 *                       objects do not include a bounding box. Otherwise
 *                       returns SCENE_OK.
 */
int
scene_copy_to_area(const scene_t *scene, void *buf, bool bounding_box,
                   bool polygon, bool velocity);

/**
 *  Deepcopy a scene object in a serialized manner to the area which is pointed
 *  to by buf. Objects added to the scene with a confidence value less than 1 << frac
 *  are ignored when low confidence objects are not allowed.
 *  Buf must be big enough to handle the whole scene.
 *
 *  @param  scene                   The scene to copy.
 *  @param  buf                     The buffer to copy buf to.
 *  @param  bounding_box            Include bounding_box data.
 *  @param  polygon                 Include polygon data.
 *  @param  velocity                Include velocity data.
 *  @param  low_confident_objects   Include objects with low confidence.
 *  @return              Returns SCENE_FAIL when at least one of the objects in
 *                       the scene do not adhere to the requested bounding_box,
 *                       polygon and/or velocity settings, i.e. the caller of
 *                       this function expects e.g. bounding box but one of the
 *                       objects do not include a bounding box. Otherwise
 *                       returns SCENE_OK.
 */
int
scene_copy_confident_to_area(const scene_t *scene, void *buf, bool bounding_box,
                             bool polygon, bool velocity, bool low_confident_objects);

/**
 *  Deserialize a scene object from a buffer pointed to by buf.
 *  @param buf           The buffer to retrieve scene from.
 */
scene_t *
scene_copy_from_area(const void *buf);

/**
 *  Serialize a scene to XML.
 *  @param  scene   The scene object to serialize.
 *  @param  xml     The output buffer where to write the XML string.
 *  @param  size    The number of bytes in the output buffer. If @e size
 *                  is zero then the @e xml argument may be NULL.
 *  @param  digits  The number of signigicant digits after the decimal
 *                  point for real number to string conversion.
 *  @return         The number of bytes that should have been written
 *                  to the output buffer if it was large enough, @b not
 *                  including the trailing NUL termination character.
 */
int
scene_serialize(const scene_t *scene, char *xml,
                unsigned size, unsigned digits);

/**
 *  Deserialize a scene from XML.
 *  @param  xml   The input XML string.
 *  @param  frac  The number of fractional bits to use in the
 *                fixed-point representation.
 *  @return       The deserialized scene. If something went wrong NULL
 *                is returned.
 */
scene_t*
scene_deserialize(const char* xml, unsigned frac);

/**
 *  Get the number of bytes allocated by the scene.
 *  @param  scene  A scene object.
 *  @return        The number of bytes allocated.
 *                 If @e scene is NULL then zero is returned.
 */
int
scene_get_size(const scene_t *scene);

/*
 * -------------------------------------------------------------
 *  Scene event management
 * -------------------------------------------------------------
 */

/**
 *  Add a new event to the scene.
 *  @param  scene  The scene object.
 *  @param  action The event action type.
 *  @return        A new event object, or NULL if the arguments
 *                 are invalid.
 */
scene_event_t*
scene_add_event(scene_t *scene, scene_action_t action);

/**
 *  Get the next event in the scene.
 *  @param  scene  The scene object.
 *  @param  event  The current event object, or NULL to get
 *                 the first event object.
 *  @return        The next event object in the scene, or NULL if
 *                 either there are no more events, or the @e scene
 *                 argument is NULL.
 */
const scene_event_t*
scene_next_event(const scene_t *scene, const scene_event_t *event);

/**
 *  Get the event action type.
 *  @param  event  The event object.
 *  @return        The action type of the event, or a negative value
 *                 if @e event is NULL.
 */
scene_action_t
scene_get_event_action(const scene_event_t *event);

/**
 *  Get the number of events in this scene.
 *  @param  scene   The object of interest.
 *  @return         The ID number of the object, or a negative number
 *                  if @e object is NULL.
 */
int
scene_get_event_num(const scene_t *scene);


/*
 * -------------------------------------------------------------
 *  Scene object management
 * -------------------------------------------------------------
 */

/**
 *  Add a new object to the scene.
 *  @param  scene  The scene object.
 *  @param  id     The unique object ID to use.
 *  @return        A new object, or NULL if either @e scene is NULL or
 *                 the ID number is held by another object in the scene.
 */
scene_object_t*
scene_add_object(scene_t *scene, int id);

/**
 *  Find an object in the scene by its ID number.
 *  @param  scene  The scene object.
 *  @param  id     The ID number to look for.
 *  @return        The corresponding object, or NULL if either
 *                 @e scene is NULL or the ID number is not in found.
 */
const scene_object_t*
scene_find_object(const scene_t *scene, int id);

/**
 *  Get the next object in the scene.
 *  @param  scene  The scene object.
 *  @param  object The current object, or NULL to get the first object.
 *  @return        The next object in the scene, or NULL if either there
 *                 are no more objects, or the @e scene argument is NULL.
 */
const scene_object_t*
scene_next_object(const scene_t *scene, const scene_object_t *object);

/**
 *  Get the ID number for an object.
 *  @param  object  The object of interest.
 *  @return         The ID number of the object, or a negative number
 *                  if @e object is NULL.
 */
int
scene_get_object_id(const scene_object_t *object);

/**
 *  Get the number of objects in this scene.
 *  @param  scene   The object of interest.
 *  @return         The ID number of the object, or a negative number
 *                  if @e object is NULL.
 */
int
scene_get_object_num(const scene_t *scene);

/*
 * -------------------------------------------------------------
 *  Scene/object/event property access
 * -------------------------------------------------------------
 */

/**
 *  Get a scene-global property value.
 *  @param  scene  The scene object.
 *  @param  prop   The property to read.
 *  @param  value  A pointer to the property value. The actual
 *                 data is held by the scene object.
 *  @return        The number of elements of the property, or
 *                 a negative value if either the property is not
 *                 found, or the arguments are invalid.
 */
int
scene_get_prop(const scene_t *scene, scene_prop_t prop, const void **value);

/**
 *  Set a scene-global property value.
 *  @param  scene  The scene object.
 *  @param  prop   The property to set.
 *  @param  value  A pointer to the property value. The data is
 *                 copied to the scene object.
 *  @param  len    The number of elements of arbitrary-length vector
 *                 properties. For scalars and fixed-length vector
 *                 properties the @e len argument is ignored.
 *  @return        A negative value if the arguments are invalid,
 *                 zero otherwise.
 */
int
scene_set_prop(scene_t *scene, scene_prop_t prop,
               const void *value, unsigned len);
/**
 *  Get an object property value.
 *  @param  object  An object in the scene.
 *  @param  prop    The property to read.
 *  @param  value   A pointer to the property value. The actual
 *                  data is held by the scene object.
 *  @return         The number of elements of the property, or
 *                  a negative value if either the property is not
 *                  found, or the arguments are invalid.
 */
int
scene_get_obj_prop(const scene_object_t *object,
                   scene_prop_t prop, const void **value);

/**
 *  Set an object property value.
 *  @param  object An object in the scene.
 *  @param  prop   The property to set.
 *  @param  value  A pointer to the property value. The data is
 *                 copied to the scene object.
 *  @param  len    The number of elements of arbitrary-length vector
 *                 properties. For scalars and fixed-length vector
 *                 properties the @e len argument is ignored.
 *  @return        A negative value if the arguments are invalid,
 *                 zero otherwise.
 */
int
scene_set_obj_prop(scene_object_t *object, scene_prop_t prop,
                   const void *value, unsigned len);
/**
 *  Get an event property value.
 *  @param  event   An event in the scene.
 *  @param  prop    The property to read.
 *  @param  value   A pointer to the property value. The actual
 *                  data is held by the scene object.
 *  @return         The number of elements of the property, or
 *                  a negative value if either the property is not
 *                  found, or the arguments are invalid.
 */
int
scene_get_evt_prop(const scene_event_t *event,
                   scene_prop_t prop, const void **value);
/**
 *  Set an event property value.
 *  @param  event  An event in the scene.
 *  @param  prop   The property to set.
 *  @param  value  A pointer to the property value. The data is
 *                 copied to the scene object.
 *  @param  len    The number of elements of arbitrary-length vector
 *                 properties. For scalars and fixed-length vector
 *                 properties the @e len argument is ignored.
 *  @return        A negative value if the arguments are invalid,
 *                 zero otherwise.
 */
int
scene_set_evt_prop(scene_event_t *event, scene_prop_t prop,
                   const void *value, unsigned len);

/**
 *  Checks if an event contains the given object id.
 *  @param  event       An event in the scene.
 *  @param  object_id   The search object id.
 *  @return             True if obj_id was found in the event.
 *                      False if obj_id was not found.
 */
bool
scene_evt_contains_obj_id(const scene_event_t *event, const int object_id);

#ifdef __cplusplus
};
#endif

#endif /* SCENE_H */
