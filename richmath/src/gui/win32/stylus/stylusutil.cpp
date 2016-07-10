#include <gui/win32/stylus/stylusutil.h>

#include <cstdio>
#include <guiddef.h>
#include <msinkaut.h>

#include <RTSCOM_i.c>
#include <msinkaut_i.c>


using namespace richmath;

//{ class StylusUtil ...

ComBase<IRealTimeStylus> StylusUtil::create_stylus_for_window(HWND hwnd) {
  ComBase<IRealTimeStylus> stylus;
  if(!HRbool(CoCreateInstance(
               CLSID_RealTimeStylus,
               nullptr,
               CLSCTX_ALL,
               IID_IRealTimeStylus,
               (void**)stylus.get_address_of())))
  {
    return stylus;
  }
  
  if(!HRbool(stylus->put_HWND((HANDLE_PTR)hwnd))) {
    stylus.reset();
    return stylus;
  }
  
  return stylus;
}

ComBase<IGestureRecognizer> StylusUtil::create_gesture_recognizer() {
  ComBase<IGestureRecognizer> recognizer;
  if(!HRbool(CoCreateInstance(
               CLSID_GestureRecognizer,
               nullptr,
               CLSCTX_ALL,
               IID_IGestureRecognizer,
               (void**)recognizer.get_address_of())))
  {
    return recognizer;
  }
  
  return recognizer;
}

ULONG StylusUtil::get_stylus_sync_plugin_count(IRealTimeStylus *stylus) {
  if(!stylus)
    return 0;
    
  ULONG count;
  if(!HRbool(stylus->GetStylusSyncPluginCount(&count)))
    return 0;
    
  return count;
}

static const char *describe_property(GUID prop) {
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_X))                         return "X";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_Y))                         return "Y";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_Z))                         return "Z";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_Z))                         return "Z";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_PACKET_STATUS))             return "PacketStatus";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_TIMER_TICK))                return "TimerTick";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_SERIAL_NUMBER))             return "SerialNumber";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_NORMAL_PRESSURE))           return "NormalPressure";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_TANGENT_PRESSURE))          return "TangentPressure";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_BUTTON_PRESSURE))           return "ButtonPressure";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_X_TILT_ORIENTATION))        return "XTiltOrientation";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_Y_TILT_ORIENTATION))        return "YTiltOrientation";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_AZIMUTH_ORIENTATION))       return "AzimuthOrientation";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_ALTITUDE_ORIENTATION))      return "AltitudeOrientation";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_TWIST_ORIENTATION))         return "TwistOrientation";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_PITCH_ROTATION))            return "PitchRotation";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_ROLL_ROTATION))             return "RollRotation";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_YAW_ROTATION))              return "YawRotation";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_WIDTH))                     return "Width";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_HEIGHT))                    return "Height";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_FINGERCONTACTCONFIDENCE))   return "FingerContactConfidence";
  if(IsEqualGUID(prop, GUID_PACKETPROPERTY_GUID_DEVICE_CONTACT_ID))         return "DeviceContactId";
  return "???";
}

static const char *describe_unit(PROPERTY_UNITS unit) {
  switch(unit) {
    case PROPERTY_UNITS_DEFAULT:     return "default unit";
    case PROPERTY_UNITS_INCHES:      return "inch";
    case PROPERTY_UNITS_CENTIMETERS: return "cm";
    case PROPERTY_UNITS_DEGREES:     return "deg";
    case PROPERTY_UNITS_RADIANS:     return "rad";
    case PROPERTY_UNITS_SECONDS:     return "sec";
    case PROPERTY_UNITS_POUNDS:      return "pounds";
    case PROPERTY_UNITS_GRAMS:       return "g";
    case PROPERTY_UNITS_SILINEAR:    return "si-linear";
    case PROPERTY_UNITS_SIROTATION:  return "si-rotation";
    case PROPERTY_UNITS_ENGLINEAR:   return "eng-linear";
    case PROPERTY_UNITS_ENGROTATION: return "eng-rotation";
    case PROPERTY_UNITS_SLUGS:       return "slugs";
    case PROPERTY_UNITS_KELVIN:      return "kelvin";
    case PROPERTY_UNITS_FAHRENHEIT:  return "fahrenheit";
    case PROPERTY_UNITS_AMPERE:      return "ampere";
    case PROPERTY_UNITS_CANDELA:     return "candela";
  }
  return "???";
}

void StylusUtil::debug_describe_packet_data_definition(IRealTimeStylus *stylus, TABLET_CONTEXT_ID tcid) {
  ULONG prop_count;
  PACKET_PROPERTY *props;
  if(HRbool(stylus->GetPacketDescriptionData(tcid, nullptr, nullptr, &prop_count, &props))) {
    fprintf(stderr, "%u properties:\n", prop_count);
    for(ULONG i = 0; i < prop_count; ++i) {
      fprintf(stderr, "  [%u] %s (%s) %ld..%ld resolution %f\n",
              i,
              describe_property(props[i].guid),
              describe_unit(props[i].PropertyMetrics.Units),
              props[i].PropertyMetrics.nLogicalMin,
              props[i].PropertyMetrics.nLogicalMax,
              (double)props[i].PropertyMetrics.fResolution);
    }
    CoTaskMemFree(props);
  }
}

void StylusUtil::debug_describe_packet_data(IRealTimeStylus *stylus, TABLET_CONTEXT_ID tcid, LONG *data) {
  ULONG prop_count;
  PACKET_PROPERTY *props;
  if(HRbool(stylus->GetPacketDescriptionData(tcid, nullptr, nullptr, &prop_count, &props))) {
    fprintf(stderr, "%u properties:\n", prop_count);
    for(ULONG i = 0; i < prop_count; ++i) {
      fprintf(stderr, "  [%u] %s = %ld\n",
              i,
              describe_property(props[i].guid),
              data[i]);
    }
    CoTaskMemFree(props);
  }
}

static const char *describe_gesture_id(int id) {
  switch(id) {
    case IAG_AllGestures:     return "IAG_AllGestures";
    case IAG_NoGesture:       return "IAG_NoGesture";
    case IAG_Scratchout:      return "IAG_Scratchout";
    case IAG_Triangle:        return "IAG_Triangle";
    case IAG_Square:          return "IAG_Square";
    case IAG_Star:            return "IAG_Star";
    case IAG_Check:           return "IAG_Check";
    case IAG_Curlicue:        return "IAG_Curlicue";
    case IAG_DoubleCurlicue:  return "IAG_DoubleCurlicue";
    case IAG_Circle:          return "IAG_Circle";
    case IAG_DoubleCircle:    return "IAG_DoubleCircle";
    case IAG_SemiCircleLeft:  return "IAG_SemiCircleLeft";
    case IAG_SemiCircleRight: return "IAG_SemiCircleRight";
    case IAG_ChevronUp:       return "IAG_ChevronUp";
    case IAG_ChevronDown:     return "IAG_ChevronDown";
    case IAG_ChevronLeft:     return "IAG_ChevronLeft";
    case IAG_ChevronRight:    return "IAG_ChevronRight";
    case IAG_ArrowUp:         return "IAG_ArrowUp";
    case IAG_ArrowDown:       return "IAG_ArrowDown";
    case IAG_ArrowLeft:       return "IAG_ArrowLeft";
    case IAG_ArrowRight:      return "IAG_ArrowRight";
    case IAG_Up:              return "IAG_Up";
    case IAG_Down:            return "IAG_Down";
    case IAG_Left:            return "IAG_Left";
    case IAG_Right:           return "IAG_Right";
    case IAG_UpDown:          return "IAG_UpDown";
    case IAG_DownUp:          return "IAG_DownUp";
    case IAG_LeftRight:       return "IAG_LeftRight";
    case IAG_RightLeft:       return "IAG_RightLeft";
    case IAG_UpLeftLong:      return "IAG_UpLeftLong";
    case IAG_UpRightLong:     return "IAG_UpRightLong";
    case IAG_DownLeftLong:    return "IAG_DownLeftLong";
    case IAG_DownRightLong:   return "IAG_DownRightLong";
    case IAG_UpLeft:          return "IAG_UpLeft";
    case IAG_UpRight:         return "IAG_UpRight";
    case IAG_DownLeft:        return "IAG_DownLeft";
    case IAG_DownRight:       return "IAG_DownRight";
    case IAG_LeftUp:          return "IAG_LeftUp";
    case IAG_LeftDown:        return "IAG_LeftDown";
    case IAG_RightUp:         return "IAG_RightUp";
    case IAG_RightDown:       return "IAG_RightDown";
    case IAG_Exclamation:     return "IAG_Exclamation";
    case IAG_Tap:             return "IAG_Tap";
    case IAG_DoubleTap:       return "IAG_DoubleTap";
  }
  return "???";
}

static const char *describe_recognition_confidence(int confidence) {
  switch(confidence) {
    case IRC_Strong:       return "strong";
    case IRC_Intermediate: return "intermediate";
    case IRC_Poor:         return "poor";
  }
  return "???";
}

void StylusUtil::debug_describe_gesture(GESTURE_DATA *gesture) {
  fprintf(
    stderr,
    "gesture %s (%x) confidence %s, %d strokes\n",
    describe_gesture_id(gesture->gestureId),
    gesture->gestureId,
    describe_recognition_confidence(gesture->recoConfidence),
    gesture->strokeCount);
}

//} ... class StylusUtil
