
/* object.c
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
#include "object.h"
#include "fizmo.h"
#include "variable.h"
#include "zpu.h"
#include "streams.h"
#include "../locales/libfizmo_locales.h"


/*@dependent@*/ static uint8_t *get_object_address(uint16_t object_number)
{
  TRACE_LOG("Reading object address from object %d.\n", object_number);
  TRACE_LOG("Object tree at $%lx.\n",
    (unsigned long int)(active_z_story->object_tree - z_mem));
  TRACE_LOG("Object size: %d bytes.\n", active_z_story->object_size);

#ifdef STRICT_Z
  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_address",
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
        "get_object_address",
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
#endif // STRICT_Z

  TRACE_LOG("Returning $%lx as object address.\n",
      (unsigned long int)(active_z_story->object_tree
      + (active_z_story->object_size * object_number) - z_mem));

  return active_z_story->object_tree
    + (active_z_story->object_size * object_number);
}


static uint8_t get_object_attribute(uint16_t object_number, uint8_t attribute_number)
{
  uint8_t *object_address = get_object_address(object_number);
  uint8_t byte_number;
  uint8_t attribute_byte_value;
  uint8_t attribute_bit_mask;

  TRACE_LOG("Reading attribute %d from object %d.\n",
      attribute_number, object_number);

#ifdef STRICT_Z
  if (object_address == NULL)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_attribute",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NULL_POINTER_RECEIVED);
    streams_latin1_output("\n");
    return 0;
  }

  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_attribute",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return 0;
  }

  if (attribute_number > active_z_story->maximum_attribute_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_attribute",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_ATTRIBUTE_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)attribute_number,
        (long int)ver);
    streams_latin1_output("\n");
    return 0;
  }
#endif // STRICT_Z

  byte_number = attribute_number >> 3;
  TRACE_LOG("Accessing attribute byte number %d.\n", byte_number);

  attribute_byte_value = object_address[byte_number];
  TRACE_LOG("Attribute byte value: $%x.\n", attribute_byte_value);

  attribute_bit_mask = (uint8_t)(0x80 >> (attribute_number & 0x7));
  TRACE_LOG("Attribute bit mask: $%x.\n", attribute_bit_mask);

  if ((attribute_byte_value & attribute_bit_mask) != 0)
    return 1;
  else
    return 0;
}


static void set_object_attribute(uint16_t object_number, uint8_t attribute_number,
    uint8_t new_attribute_value)
{
  uint8_t *object_address = get_object_address(object_number);
  uint8_t byte_number;
  uint8_t attribute_bit_mask;

  TRACE_LOG("Setting attribute %d from object %d to %d.\n",
      attribute_number, object_number, new_attribute_value);

#ifdef STRICT_Z
  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "set_object_attribute",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return;
  }

  if (attribute_number > active_z_story->maximum_attribute_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "set_object_attribute",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_ATTRIBUTE_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)attribute_number,
        (long int)ver);
    streams_latin1_output("\n");
    return;
  }

  if (object_address == NULL)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "set_object_attribute",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NULL_POINTER_RECEIVED);
    streams_latin1_output("\n");
    return;
  }
#endif // STRICT_Z

  byte_number = attribute_number >> 3;
  TRACE_LOG("Accessing attribute byte number %d.\n", byte_number);

  TRACE_LOG("Previous attribute byte value: $%x.\n",
      object_address[byte_number]);

  attribute_bit_mask = (uint8_t)(0x80 >> (attribute_number & 0x7));

  if (new_attribute_value == 0)
  {
    attribute_bit_mask = ~attribute_bit_mask;
    object_address[byte_number] &= attribute_bit_mask;
  }
  else
    object_address[byte_number] |= attribute_bit_mask;

  TRACE_LOG("Final attribute byte value: $%x.\n", object_address[byte_number]);
}


static uint16_t get_object_node_number(uint16_t object_number, uint8_t node_type)
{
  uint8_t *object_address = get_object_address(object_number);

#ifdef STRICT_Z
  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_node_number",
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
        "get_object_node_number",
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

  if (object_address == NULL)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_node_number",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NULL_POINTER_RECEIVED);
    streams_latin1_output("\n");
    return 0;
  }

  if ((node_type != OBJECT_NODE_PARENT) &&
      (node_type != OBJECT_NODE_SIBLING) &&
      (node_type != OBJECT_NODE_CHILD))
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_object_node_number",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_INVALID_NODE_TYPE_P0D,
        (long int)node_type);
    streams_latin1_output("\n");

    return 0;
  }
#endif // STRICT_Z

  if (ver <= 3)
    return
      *(object_address + active_z_story->object_node_number_index + node_type);

  else
    return load_word(object_address
        + active_z_story->object_node_number_index
        + node_type*2);
}


static void set_object_node_number(uint16_t object_number, uint8_t node_type,
    uint16_t new_node_number)
{
  uint8_t *object_address = get_object_address(object_number);

#ifdef STRICT_Z
  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "set_object_node_number",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return;
  }

  if ((object_number > active_z_story->maximum_object_number)
      || (new_node_number > active_z_story->maximum_object_number))
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "set_object_node_number",
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

  if ((node_type != OBJECT_NODE_PARENT) &&
      (node_type != OBJECT_NODE_SIBLING) &&
      (node_type != OBJECT_NODE_CHILD))
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "set_object_node_number",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_INVALID_NODE_TYPE_P0D,
        (long int)node_type);
    streams_latin1_output("\n");
    return;
  }

  if (object_address == NULL)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "set_object_node_number",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NULL_POINTER_RECEIVED);
    streams_latin1_output("\n");
    return;
  }
#endif // STRICT_Z

  if (ver <= 3)
    *(object_address + active_z_story->object_node_number_index + node_type)
      = new_node_number;

  else
    store_word
      (object_address + active_z_story->object_node_number_index + node_type*2,
       new_node_number);
}


/*@dependent@*/ uint8_t *get_objects_property_table(uint16_t object_number)
{
  uint8_t *object_address = get_object_address(object_number);

#ifdef STRICT_Z
  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "get_objects_property_table",
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
        "get_objects_property_table",
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

  if (object_address == NULL)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "set_object_node_number",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_NULL_POINTER_RECEIVED);
    streams_latin1_output("\n");
    return 0;
  }
#endif // STRICT_Z

  TRACE_LOG("Property table of object %d is at $%x.\n",
      object_number,
      load_word(object_address + active_z_story->object_property_index));

  return
    z_mem + load_word(object_address + active_z_story->object_property_index);
}


static void unlink_object(uint16_t object_number)
{
  uint16_t parent_node;
  uint16_t sibling_node;
  uint16_t next_sibling_node;

#ifdef STRICT_Z
  if (object_number == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "unlink_object",
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
        "unlink_object",
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
#endif // STRICT_Z

  TRACE_LOG("Unlinking object %d.\n", object_number);

  // In order to unlink an object we have to check if it is the direct
  // child of it's parent. If so, we have to delete this reference and
  // substitute it with the reference to the next sibling (if it exists).
  // We also have to remove the sibling's reference to this object and
  // replace it with this object's sibling reference.

  // In case the object is already unlinked, there's nothing to be done.
  TRACE_LOG("Current parent of %d is %d.\n", object_number,
      get_object_node_number(object_number, OBJECT_NODE_PARENT));

  parent_node = get_object_node_number(object_number, OBJECT_NODE_PARENT);
  if (parent_node == 0)
    return;

  // Check if this object is the first child of it's parent.
  if (get_object_node_number(parent_node, OBJECT_NODE_CHILD) == object_number)
  {
    // If it is, we replace our reference with  reference to the next
    // sibling. That's everything we'll have to do, since we're the first
    // child, no other sibling can have a reference to this object.
    TRACE_LOG("New child of %d is %d.\n", parent_node,
        get_object_node_number(object_number, OBJECT_NODE_SIBLING));
    set_object_node_number(parent_node, OBJECT_NODE_CHILD,
        get_object_node_number(object_number, OBJECT_NODE_SIBLING));
  }

  else
  {
    // In case we're not the first child, we'll have to find the sibling
    // that contains the reference to our node.
    TRACE_LOG("Looking for node with sibling == %d.\n", object_number);
    next_sibling_node = get_object_node_number(parent_node, OBJECT_NODE_CHILD);
    do
    {
      sibling_node = next_sibling_node;
      next_sibling_node
        = get_object_node_number(sibling_node, OBJECT_NODE_SIBLING);
    }
    while ((next_sibling_node != object_number) && (next_sibling_node != 0));

    if (next_sibling_node == 0)
    {
      TRACE_LOG("Warning: Parent has no sibling with reference to node.");
    }
    else
    {
      TRACE_LOG("New sibling of %d is %d.\n", sibling_node,
          get_object_node_number(object_number, OBJECT_NODE_SIBLING));
      set_object_node_number(
          sibling_node,
          OBJECT_NODE_SIBLING,
          get_object_node_number(object_number, OBJECT_NODE_SIBLING));
    }
  }

  TRACE_LOG("Seeting parent and sibling of %d to 0.\n", object_number);
  set_object_node_number(object_number, OBJECT_NODE_SIBLING, 0);
  set_object_node_number(object_number, OBJECT_NODE_PARENT, 0);
}


void opcode_get_sibling(void)
{
  uint16_t sibling_number;

  TRACE_LOG("Opcode: GET_SIBLING.\n");

  read_z_result_variable();

#ifdef STRICT_Z
  if (op[0]== 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_sibling",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    evaluate_branch(0);
    return;
  }

  if (op[0] > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_sibling",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[0],
        (long int)ver);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    evaluate_branch(0);
    return;
  }
#endif // STRICT_Z

  sibling_number = get_object_node_number(op[0], OBJECT_NODE_SIBLING);

  TRACE_LOG("Checking if sibling of object %d (which is %d) is not 0.\n",
      op[0], sibling_number);

  set_variable(z_res_var, sibling_number, false);
  evaluate_branch(sibling_number != 0 ? (uint8_t)1 : (uint8_t)0);
}


void opcode_get_child(void)
{
  uint16_t child_number;

  TRACE_LOG("Opcode: GET_CHILD.\n");

  read_z_result_variable();

#ifdef STRICT_Z
  if (op[0]== 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_child",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    evaluate_branch(0);
    return;
  }

  if (op[0] > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_child",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[0],
        (long int)ver);
    streams_latin1_output("\n");

    set_variable(z_res_var, 0, false);
    evaluate_branch(0);
    return;
  }
#endif // STRICT_Z

  child_number = get_object_node_number(op[0], OBJECT_NODE_CHILD);

  TRACE_LOG("Checking if child of object %d (which is %d) is not 0.\n",
      op[0], child_number);

  set_variable(z_res_var, child_number, false);
  evaluate_branch(child_number != 0 ? (uint8_t)1 : (uint8_t)0);
}


void opcode_get_parent(void)
{
  uint16_t parent_number;

  TRACE_LOG("Opcode: GET_PARENT.\n");

  read_z_result_variable();

#ifdef STRICT_Z
  if (op[0]== 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_get_parent",
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
        "opcode_get_parent",
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
#endif // STRICT_Z

  parent_number = get_object_node_number(op[0], OBJECT_NODE_PARENT);

  TRACE_LOG("Checking if parent of object %d (which is %d) is not 0.\n",
      op[0], parent_number);

  set_variable(z_res_var, parent_number, false);
}


void opcode_jin(void)
{
  TRACE_LOG("Opcode: JIN.\n");
  TRACE_LOG("Checking if parent of object %d is %d.\n", op[0], op[1]);

#ifdef STRICT_Z
  if ((op[0] == 0) || (op[1] == 0))
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_jin",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");

    evaluate_branch(op[0] == op[1] ? 1 : 0);
    return;
  }

  if (op[0] > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_jin",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[0],
        (long int)ver);
    streams_latin1_output("\n");

    evaluate_branch(0);
    return;
  }

  if (op[1] > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_jin",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[1],
        (long int)ver);
    streams_latin1_output("\n");

    evaluate_branch(0);
    return;
  }
#endif // STRICT_Z

  evaluate_branch(
      get_object_node_number(
        op[0], OBJECT_NODE_PARENT) == op[1] ? (uint8_t)1 : (uint8_t)0);
}


void opcode_set_attr(void)
{
  TRACE_LOG("Opcode: SET_ATTR.\n");
  TRACE_LOG("Setting attribute %d in object %d.\n", op[1], op[0]);

#ifdef STRICT_Z
  if (op[0] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_set_attr",
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
        "opcode_set_attr",
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

  if (op[1] > active_z_story->maximum_attribute_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_set_attr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_ATTRIBUTE_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[1],
        (long int)ver);
    streams_latin1_output("\n");
    return;
  }
#endif // STRICT_Z

  set_object_attribute(op[0], op[1], 1);
}


void opcode_test_attr(void)
{
  TRACE_LOG("Opcode: TEST_ATTR.\n");
  TRACE_LOG("Testing object %d for attribute %d.\n", op[0], op[1]);

#ifdef STRICT_Z
  if (op[0] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_test_attr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    evaluate_branch(0);
    return;
  }

  if (op[0] > active_z_story->maximum_object_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_test_attr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[0],
        (long int)ver);
    streams_latin1_output("\n");
    evaluate_branch(0);
    return;
  }

  if (op[1] > active_z_story->maximum_attribute_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_test_attr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_ATTRIBUTE_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[1],
        (long int)ver);
    streams_latin1_output("\n");
    evaluate_branch(0);
    return;
  }
#endif // STRICT_Z

  evaluate_branch(get_object_attribute(op[0], op[1]));
}


void opcode_insert_obj(void)
{
  TRACE_LOG("Opcode: INSERT_OBJ.\n");

  TRACE_LOG("Inserting object %d as first child in object %d.\n",op[0],op[1]);

#ifdef STRICT_Z
  if ((op[0] == 0) || (op[1] == 0))
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_remove_attr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return;
  }
#endif // STRICT_Z

  unlink_object(op[0]);

  TRACE_LOG("New sibling of %d is %d.\n", op[0], 
      get_object_node_number(op[1], OBJECT_NODE_CHILD));

  set_object_node_number(
      op[0],
      OBJECT_NODE_SIBLING,
      get_object_node_number(op[1], OBJECT_NODE_CHILD));

  TRACE_LOG("New child of %d is %d.\n", op[1], op[0]);
  set_object_node_number(op[1], OBJECT_NODE_CHILD, op[0]);

  TRACE_LOG("New parent of %d is %d.\n", op[0], op[1]);
  set_object_node_number(op[0], OBJECT_NODE_PARENT, op[1]);
}


void opcode_clear_attr(void)
{
  TRACE_LOG("Opcode: CLEAR_ATTR.\n");

  TRACE_LOG("Clearing attribute %d of object %d.\n", op[1], op[0]);

#ifdef STRICT_Z
  if (op[0] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_clear_attr",
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
        "opcode_clear_attr",
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

  if (op[1] > active_z_story->maximum_attribute_number)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_clear_attr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_ATTRIBUTE_NUMBER_P0D_NOT_ALLOWED_IN_STORY_VERSION_P1D,
        (long int)op[1],
        (long int)ver);
    streams_latin1_output("\n");
    return;
  }
#endif // STRICT_Z
  set_object_attribute(op[0], op[1], 0);
}


void opcode_remove_obj(void)
{
  TRACE_LOG("Opcode: REMOVE_OBJ.\n");

  TRACE_LOG("Removing object %d.\n", op[0]);

#ifdef STRICT_Z
  if (op[0] == 0)
  {
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_WARNING_FOR_P0S_AT_P0X,
        "opcode_remove_attr",
        (unsigned int)(current_instruction_location - z_mem));
    streams_latin1_output(" ");
    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_OBJECT_NUMBER_0_IS_NOT_VALID);
    streams_latin1_output("\n");
    return;
  }
#endif // STRICT_Z

  unlink_object(op[0]);
}

