
/* property.c
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2009-2017 Christoph Ender.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "property.h"
#include "fizmo.h"
#include "variable.h"
#include "object.h"
#include "zpu.h"
#include "streams.h"
#include "config.h" // for IGNORE_TOO_LONG_PROPERTIES_ERROR
#include "../locales/libfizmo_locales.h"


static uint8_t get_property_length(uint8_t *property)
{
  uint8_t length_and_number;
  uint8_t length;

#ifdef STRICT_Z
  if (property == NULL)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_length",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NULL_POINTER_RECEIVED);
    streams_latin1_output("\n");
    return 0;
  }
#endif // STRICT_Z

  if (ver <= 3)
  {
    return (*property >> 5) + 1;
  }
  else
  {
    length_and_number = *property;
    TRACE_LOG("Property-byte: %x.\n", *property);

    if ((length_and_number & 0x80) != 0)
    {
      TRACE_LOG("Property-byte: %x.\n", *(property+1));
      // 12.4.2.1 If the top bit (bit 7) of the first size byte is set,
      // then there are two size-and-number bytes as follows. In the
      // first byte, bits 0 to 5 contain the property number; bit 6 is
      // undetermined (it is clear in all Infocom or Inform story files);
      // bit 7 is set. In the second byte, bits 0 to 5 contain the property
      // data length, counting in bytes; bit 6 is undetermined (it is set
      // in Infocom story files, but clear in Inform ones); bit 7 is always
        // set.
      length = *(++property) & 0x3f;

      // 12.4.2.1.1  A value of 0 as property data length (in the
      // second byte) should be interpreted as a length of 64. (Inform
      // can compile such properties.)
      if (length == 0)
        length = 64;
    }
    else
    {
      // 12.4.2.2 If the top bit (bit 7) of the first size byte is clear,
      // then there is only one size-and-number byte. Bits 0 to 5 contain
      // the property number; bit 6 is either clear to indicate a property
      // data length of 1, or set to indicate a length of 2; bit 7 is clear.
      length = (length_and_number & 0x40) != 0 ? (uint8_t)2 : (uint8_t)1;
    }

    return length;
  }
}


static inline uint8_t get_property_length_code_size(uint8_t *property)
{
#ifdef STRICT_Z
  if (property == NULL)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_length_code_size",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NULL_POINTER_RECEIVED);
    streams_latin1_output("\n");
    return 0;
  }
#endif // STRICT_Z

  // 12.4.2.1 If the top bit (bit 7) of the first size byte is set,
  // then there are two size-and-number bytes [...]
  if ((ver >= 4) && ((*property & 0x80) != 0))
    return 2;
  else
    return 1;
}


static uint16_t get_default_property_value(uint16_t property_number)
{
#ifdef STRICT_Z
  if (property_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_default_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return 0;
  }

  if (property_number > active_z_story->maximum_property_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_default_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)property_number,
        (long int)ver);
    streams_latin1_output("\n");
    return 0;
  }
#endif // STRICT_Z

  return load_word(active_z_story->property_defaults + property_number*2);
}


/*@dependent@*/ /*@null@*/ static uint8_t *get_objects_first_property(
    uint16_t object_number)
{
  uint8_t *property_table_index = get_objects_property_table(object_number);
  uint8_t length_and_number;

#ifdef STRICT_Z
  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_objects_first_property",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return NULL;
  }

  if (object_number > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_objects_first_property",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)object_number,
        (long int)ver);
    streams_latin1_output("\n");
    return NULL;
  }

  if (property_table_index == NULL)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_objects_first_property",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NULL_POINTER_RECEIVED);
    streams_latin1_output("\n");
    return NULL;
  }
#endif // STRICT_Z

  TRACE_LOG("Property table located at $%lx.\n",
    (unsigned long int)(property_table_index - z_mem));

  TRACE_LOG("Skipping property name (%d+1 bytes).\n",
      (*property_table_index)*2);
  property_table_index += (*property_table_index)*2 + 1;

  length_and_number = *property_table_index;

  if (length_and_number == 0)
  {
    TRACE_LOG("Object %d has no properties.\n", object_number);
    return NULL;
  }
  else
  {
#ifdef ENABLE_TRACING
    if (ver <= 3)
    {
      TRACE_LOG("Found property %d.\n", length_and_number & 0x1f);
    }
    else
    {
      TRACE_LOG("Found property %d.\n", length_and_number & 0x3f);
    }
#endif

    return property_table_index;
  }
}


/*@null@*/ /*@dependent@*/ static uint8_t *get_objects_next_property(
    /*@dependent@*/ uint8_t *property_index)
{
  uint8_t length_code_size;
  uint8_t length;
  uint8_t length_and_number;

#ifdef STRICT_Z
  if (property_index == NULL)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_objects_next_property",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NULL_POINTER_RECEIVED);
    streams_latin1_output("\n");
    return NULL;
  }
#endif // STRICT_Z

  TRACE_LOG("Looking for next property behind property %d.\n",
      *property_index & 0x1f);

  if (*property_index == 0)
    return NULL;

  length_code_size = get_property_length_code_size(property_index);
  length = get_property_length(property_index);
  TRACE_LOG("Property %d has a length of %d and a length-code-size of %d.\n",
      *property_index & 0x1f, length, length_code_size);

  property_index += length + length_code_size;
  length_and_number = *property_index;


  if (length_and_number == 0)
  {
    TRACE_LOG("No property behind current property.\n");
    return NULL;
  }
  else
  {
#ifdef ENABLE_TRACING
    if (ver <= 3)
    {
      TRACE_LOG("Returning address of property %d.\n", *property_index & 0x1f);
    }
    else
    {
      TRACE_LOG("Returning address of property %d.\n", *property_index & 0x3f);
    }
#endif

    return property_index;
  }
}


/*@null@*/ /*@dependent@*/ static uint8_t *get_object_property(
    uint16_t object_number, uint16_t property_number)
{
  uint8_t *property_index;
  int current_prop_number;

#ifdef STRICT_Z
  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_property",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return NULL;
  }

  if (object_number > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_property",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)object_number,
        (long int)ver);
    streams_latin1_output("\n");
    return NULL;
  }

  if (property_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_property",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return NULL;
  }

  if (property_number > active_z_story->maximum_property_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_property",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)property_number,
        (long int)ver);
    streams_latin1_output("\n");
    return NULL;
  }
#endif // STRICT_Z

  property_index = get_objects_first_property(object_number);
  if (property_index != NULL)
  {
    if (ver >= 4)
      current_prop_number = (int)(*property_index & 0x3f);
    else
      current_prop_number = (int)(*property_index & 0x1f);
    TRACE_LOG("Processing property %d.\n", current_prop_number);
  }

  while (
      (property_index != NULL)
      &&
      // current_prop_number is defined, since property_index != NULL.
      (/*@-usedef@*/ current_prop_number /*@+usedef@*/ != (int)property_number)
      )
  {
      property_index = get_objects_next_property(property_index);
      if (property_index != NULL)
      {
        if (ver >= 4)
          current_prop_number = (int)(*property_index & 0x3f);
        else
          current_prop_number = (int)(*property_index & 0x1f);
        TRACE_LOG("Processing property %d.\n", current_prop_number);
      }
  }

  return property_index;
}


static uint16_t get_property_value(uint16_t object_number,
    uint8_t property_number)
{
  uint8_t *property_table_index;
  uint8_t length;
  uint8_t length_code_size;

  TRACE_LOG("Reading property %d from object %d.\n", property_number,
      object_number);

#ifdef STRICT_Z
  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return 0;
  }

  if (object_number > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)object_number,
        (long int)ver);
    streams_latin1_output("\n");
    return 0;
  }

  if (property_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return 0;
  }

  if (property_number > active_z_story->maximum_property_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)property_number,
        (long int)ver);
    streams_latin1_output("\n");
    return 0;
  }
#endif // STRICT_Z

  property_table_index = get_object_property(object_number, property_number);

  if (property_table_index == NULL)
  {
#ifdef STRICT_Z
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NO_PROPERTY_P0D_FOR_OBJECT_P1D);
    streams_latin1_output("\n");
#endif // STRICT_Z
    return 0;
  }

  length = get_property_length(property_table_index);
  length_code_size = get_property_length_code_size(property_table_index);
  TRACE_LOG("Property length: %d\n", length);

  if (length == 1)
    return *(property_table_index + length_code_size);
  else if (length == 2)
    return load_word(property_table_index + length_code_size);
  else      
  {
#ifdef IGNORE_TOO_LONG_PROPERTIES_ERROR
    return load_word(property_table_index + length_code_size);
#else
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_CANNOT_READ_PROPERTIES_WITH_A_LENGTH_GREATER_THAN_2, -1);

    /*@-unreachable@*/
    return 0; // To make compiler happy
    /*@+unreachable@*/
#endif // IGNORE_TOO_LONG_PROPERTIES_ERROR
  }
}


static void set_property_value(uint16_t object_number,
    uint8_t property_number, uint16_t new_value)
{
  uint8_t *property_table_index;
  uint8_t length;
  uint8_t length_code_size;

  TRACE_LOG("Setting property %d from object %d to $%x.\n", property_number,
      object_number, new_value);

#ifdef STRICT_Z
  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return;
  }

  if (object_number > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)object_number,
        (long int)ver);
    streams_latin1_output("\n");
    return;
  }

  if (property_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return;
  }

  if (property_number > active_z_story->maximum_property_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)property_number,
        (long int)ver);
    streams_latin1_output("\n");
    return;
  }
#endif // STRICT_Z

  property_table_index = get_object_property(object_number, property_number);

  if (property_table_index == NULL)
  {
#ifdef STRICT_Z
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "set_property_value",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NO_PROPERTY_P0D_FOR_OBJECT_P1D,
        (long int)property_number,
        (long int)object_number);
    streams_latin1_output("\n");
#endif // STRICT_Z
    return;
  }

  length = get_property_length(property_table_index);
  length_code_size = get_property_length_code_size(property_table_index);

  if (length == 1)
    *(property_table_index + length_code_size) = new_value & 0xff;
  else if (length == 2)
    store_word(property_table_index + length_code_size, new_value);
  else      
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_CANNOT_READ_PROPERTIES_WITH_A_LENGTH_GREATER_THAN_2,
        -1);

  return;
}


void opcode_get_prop(void)
{
  TRACE_LOG("Opcode: GET_PROP.\n");

  read_z_result_variable();

  TRACE_LOG("Reading property #%x of object %x.\n", op[1], op[0]);

#ifdef STRICT_Z
  if (op[0] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    return;
  }

  if (op[0] > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[0],
        (long int)ver);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    return;
  }

  if (op[1] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    return;
  }

  if (op[1] > active_z_story->maximum_property_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[1],
        (long int)ver);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    return;
  }
#endif // STRICT_Z

  // Verify if the object has the requested property.
  if (get_object_property(op[0], op[1]) != NULL)
  {
    set_variable(z_res_var, get_property_value(op[0], op[1]), false);
  }
  else
  {
    TRACE_LOG("Property %d not found, returning default value %d.\n",
        op[1],
        get_default_property_value(op[1]));
    set_variable(z_res_var, get_default_property_value(op[1]), false);
  }
}


void opcode_put_prop(void)
{
  TRACE_LOG("Opcode: PUT_PROP.\n");
  TRACE_LOG("Putting %x into property %d of object %d.\n",op[2],op[1],op[0]);

#ifdef STRICT_Z
  if (op[0] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_put_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return;
  }

  if (op[0] > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_put_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[0],
        (long int)ver);
    streams_latin1_output("\n");
    return;
  }

  if (op[1] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_put_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return;
  }

  if (op[1] > active_z_story->maximum_property_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_put_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[1],
        (long int)ver);
    streams_latin1_output("\n");
    return;
  }
#endif // STRICT_Z

  (void)set_property_value(op[0], op[1], op[2]);
}


void opcode_get_prop_addr(void)
{
  uint8_t *property_address;

  TRACE_LOG("Opcode: GET_PROP_ADDR.\n");
  TRACE_LOG("Retrieving address of property %d in object %d.\n", op[1], op[0]);

  read_z_result_variable();

#ifdef STRICT_Z
  if (op[0] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_prop_addr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    return;
  }

  if (op[0] > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_prop_addr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[0],
        (long int)ver);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    return;
  }

  if (op[1] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_prop_addr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    return;
  }

  if (op[1] > active_z_story->maximum_property_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_prop_addr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[1],
        (long int)ver);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    return;
  }
#endif // STRICT_Z

  property_address = get_object_property(op[0], op[1]);

  if (property_address == NULL)
    set_variable(z_res_var, 0, false);
  else
  {
    if ( (ver >= 4) && ((*property_address & 0x80) != 0) )
      property_address += 2;
    else
      property_address += 1;
    TRACE_LOG("Returning %lx.\n",
      (long unsigned int)(property_address - z_mem));
    set_variable(z_res_var, (uint16_t)(property_address - z_mem), false);
  }
}


void opcode_get_prop_len(void)
{
  uint8_t *property_address;
  uint16_t result;

  TRACE_LOG("Opcode: GET_PROP_LEN.\n");
  TRACE_LOG("Reading length of property at address $%x.\n", op[0]);
    
  read_z_result_variable();

  if (op[0] == 0)
  {
    // From the 1.1 standard: @get_prop_len 0 must return 0. This is
    // required by some Infocom games and files generated by old
    // versions of Inform.
    result = 0;
  }
  else
  {
    property_address = z_mem + op[0] - 1;
    if ( (ver >= 4) && ((*property_address & 0x80) != 0) )
      property_address--;
    result = get_property_length(property_address);

    TRACE_LOG("Setting variable with code %d to %d.\n", z_res_var, result);
  }

  set_variable(z_res_var, result, false);
}


void opcode_get_next_prop(void)
{
  uint8_t *property_index;

  TRACE_LOG("Opcode: GET_NEXT_PROP\n");

  read_z_result_variable();

#ifdef STRICT_Z
  if (op[0] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_next_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return;
  }

  if (op[0] > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_next_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[0],
        (long int)ver);
    streams_latin1_output("\n");
    return;
  }

  if (op[1] > active_z_story->maximum_property_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_next_prop",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_PROPERTY_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[1],
        (long int)ver);
    streams_latin1_output("\n");
    return;
  }
#endif // STRICT_Z

  if (op[1] == 0)
  {
    TRACE_LOG("Looking for first property of object %d.\n", op[0]);
    // Retrieve object's first property
    property_index = get_objects_first_property(op[0]);
    if (property_index == NULL)
    {
      TRACE_LOG("Empty property list, returning 0.\n");
      set_variable(z_res_var, 0, false);
    }
    else
    {
      TRACE_LOG("First property of object %d is %d.\n",
          op[0], *property_index & 0x1f);
      set_variable(z_res_var, *property_index & 0x1f, false);
    }
  }
  else
  {
    property_index = get_object_property(op[0], op[1]);
    if (property_index == NULL)
    {
      set_variable(z_res_var, 0, false);
    }
    else
    {
      property_index = get_objects_next_property(property_index);
      TRACE_LOG("Looking for property behind property %d.\n", op[1]);
      if (property_index == NULL)
      {
        TRACE_LOG("No more properties in object %d, returning 0.\n", op[0]);
        set_variable(z_res_var, 0, false);
      }
      else
      {
        TRACE_LOG("Returning %d od resulting property number.\n",
            *property_index & 0x1f);
        set_variable(z_res_var, *property_index & 0x1f, false);
      }
    }
  }
}

