// Filename: pfmTrans.cxx
// Created by:  drose (23Dec10)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) Carnegie Mellon University.  All rights reserved.
//
// All use of this software is subject to the terms of the revised BSD
// license.  You should have received a copy of this license along
// with this source code in a file named "LICENSE."
//
////////////////////////////////////////////////////////////////////

#include "pfmTrans.h"
#include "config_pfm.h"
#include "pfmFile.h"
#include "pystub.h"
#include "texture.h"
#include "texturePool.h"
#include "pointerTo.h"
#include "string_utils.h"
#include "pandaFileStream.h"

////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::Constructor
//       Access: Public
//  Description:
////////////////////////////////////////////////////////////////////
PfmTrans::
PfmTrans() {
  _got_transform = false;
  _transform = LMatrix4::ident_mat();

  add_transform_options();

  set_program_description
    ("pfm-trans reads an pfm file and transforms it, filters it, "
     "operates on it, writing the output to another pfm file.  A pfm "
     "file contains a 2-d table of floating-point values.");

  add_option
    ("r", "", 0,
     "Reverses the rows of the pfm data vertically.",
     &PfmTrans::dispatch_none, &_got_reverse);

  add_option
    ("z", "", 0,
     "Treats (0,0,0) in the pfm file as a special don't-touch value.",
     &PfmTrans::dispatch_none, &_got_zero_special);

  add_option
    ("resize", "width,height", 0,
     "Resamples the pfm file to scale it to the indicated grid size.  "
     "A simple box filter is applied during the scale.  Don't confuse this "
     "with -TS, which scales the individual point values, but doesn't "
     "change the number of points.",
     &PfmTrans::dispatch_int_pair, &_got_resize, &_resize);

  add_option
    ("o", "filename", 50,
     "Specify the filename to which the resulting pfm file will be written.  "
     "This is only valid when there is only one input pfm file on the command "
     "line.  If you want to process multiple files simultaneously, you must "
     "use -d.",
     &PfmTrans::dispatch_filename, &_got_output_filename, &_output_filename);

  add_option
    ("d", "dirname", 50,
     "Specify the name of the directory in which to write the processed pfm "
     "files.  If you are processing only one pfm file, this may be omitted "
     "in lieu of the -o option.",
     &PfmTrans::dispatch_filename, &_got_output_dirname, &_output_dirname);

  add_option
    ("vis", "filename.bam", 60,
     "Generates a bam file that represents a visualization of the pfm file "
     "as a 3-D geometric mesh.  If -vistex is specified, the mesh is "
     "textured.",
     &PfmTrans::dispatch_filename, &_got_vis_filename, &_vis_filename);

  add_option
    ("visinv", "", 60,
     "Inverts the visualization, generating a uniform 2-d mesh with the "
     "3-d depth values encoded in the texture coordinates.",
     &PfmTrans::dispatch_none, &_got_vis_inverse);

  add_option
    ("vis2d", "", 60,
     "Respect only the first two components of each depth value, ignoring z.",
     &PfmTrans::dispatch_none, &_got_vis_2d);

  add_option
    ("vistex", "texture.jpg", 60,
     "Specifies the name of the texture to apply to the visualization.",
     &PfmTrans::dispatch_filename, &_got_vistex_filename, &_vistex_filename);
}


////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::run
//       Access: Public
//  Description:
////////////////////////////////////////////////////////////////////
void PfmTrans::
run() {
  if (_got_vis_filename) {
    _mesh_root = NodePath("mesh_root");
  }

  Filenames::const_iterator fi;
  for (fi = _input_filenames.begin(); fi != _input_filenames.end(); ++fi) {
    PfmFile file;
    if (!file.read(*fi)) {
      nout << "Cannot read " << *fi << "\n";
      exit(1);
    }
    if (!process_pfm(*fi, file)) {
      exit(1);
    }
  }

  if (_got_vis_filename) {
    _mesh_root.write_bam_file(_vis_filename);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::process_pfm
//       Access: Public
//  Description: Handles a single pfm file.
////////////////////////////////////////////////////////////////////
bool PfmTrans::
process_pfm(const Filename &input_filename, PfmFile &file) {
  file.set_zero_special(_got_zero_special);
  file.set_vis_inverse(_got_vis_inverse);
  file.set_vis_2d(_got_vis_2d);

  if (_got_resize) {
    file.resize(_resize[0], _resize[1]);
  }

  if (_got_reverse) {
    file.reverse_rows();
  }

  if (_got_transform) {
    file.xform(_transform);
  }

  if (_got_vis_filename) {
    NodePath mesh = file.generate_vis_mesh(true);
    if (_got_vistex_filename) {
      PT(Texture) tex = TexturePool::load_texture(_vistex_filename);
      if (tex == NULL) {
        nout << "Couldn't find " << _vistex_filename << "\n";
      } else {
        mesh.set_texture(tex);
      }
      if (tex->has_alpha(tex->get_format())) {
        mesh.set_transparency(TransparencyAttrib::M_dual);
      }
    }
    mesh.set_name(input_filename.get_basename_wo_extension());
    mesh.reparent_to(_mesh_root);
  }

  Filename output_filename;
  if (_got_output_filename) {
    output_filename = _output_filename;
  } else if (_got_output_dirname) {
    output_filename = Filename(_output_dirname, input_filename.get_basename());
  }

  if (!output_filename.empty()) {
    return file.write(output_filename);
  }

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::add_transform_options
//       Access: Public
//  Description: Adds -TS, -TT, etc. as valid options for this
//               program.  If the user specifies one of the options on
//               the command line, the data will be transformed when
//               the egg file is written out.
////////////////////////////////////////////////////////////////////
void PfmTrans::
add_transform_options() {
  add_option
    ("TS", "sx[,sy,sz]", 49,
     "Scale the model uniformly by the given factor (if only one number "
     "is given) or in each axis by sx, sy, sz (if three numbers are given).",
     &PfmTrans::dispatch_scale, &_got_transform, &_transform);

  add_option
    ("TR", "x,y,z", 49,
     "Rotate the model x degrees about the x axis, then y degrees about the "
     "y axis, and then z degrees about the z axis.",
     &PfmTrans::dispatch_rotate_xyz, &_got_transform, &_transform);

  add_option
    ("TA", "angle,x,y,z", 49,
     "Rotate the model angle degrees counterclockwise about the given "
     "axis.",
     &PfmTrans::dispatch_rotate_axis, &_got_transform, &_transform);

  add_option
    ("TT", "x,y,z", 49,
     "Translate the model by the indicated amount.\n\n"
     "All transformation options (-TS, -TR, -TA, -TT) are cumulative and are "
     "applied in the order they are encountered on the command line.",
     &PfmTrans::dispatch_translate, &_got_transform, &_transform);
}

////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::handle_args
//       Access: Protected, Virtual
//  Description: Does something with the additional arguments on the
//               command line (after all the -options have been
//               parsed).  Returns true if the arguments are good,
//               false otherwise.
////////////////////////////////////////////////////////////////////
bool PfmTrans::
handle_args(ProgramBase::Args &args) {
  if (args.empty()) {
    nout << "You must specify the pfm file(s) to read on the command line.\n";
    return false;
  }

  if (_got_output_filename && args.size() == 1) {
    if (_got_output_dirname) {
      nout << "Cannot specify both -o and -d.\n";
      return false;
    }
    
  } else {
    if (_got_output_filename) {
      nout << "Cannot use -o when multiple pfm files are specified.\n";
      return false;
    }
  }

  Args::const_iterator ai;
  for (ai = args.begin(); ai != args.end(); ++ai) {
    _input_filenames.push_back(Filename::from_os_specific(*ai));
  }

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::dispatch_scale
//       Access: Protected, Static
//  Description: Handles -TS, which specifies a scale transform.  Var
//               is an LMatrix4.
////////////////////////////////////////////////////////////////////
bool PfmTrans::
dispatch_scale(const string &opt, const string &arg, void *var) {
  LMatrix4 *transform = (LMatrix4 *)var;

  vector_string words;
  tokenize(arg, words, ",");

  PN_stdfloat sx, sy, sz;

  bool okflag = false;
  if (words.size() == 3) {
    okflag =
      string_to_float(words[0], sx) &&
      string_to_float(words[1], sy) &&
      string_to_float(words[2], sz);

  } else if (words.size() == 1) {
    okflag =
      string_to_float(words[0], sx);
    sy = sz = sx;
  }

  if (!okflag) {
    nout << "-" << opt
         << " requires one or three numbers separated by commas.\n";
    return false;
  }

  *transform = (*transform) * LMatrix4::scale_mat(sx, sy, sz);

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::dispatch_rotate_xyz
//       Access: Protected, Static
//  Description: Handles -TR, which specifies a rotate transform about
//               the three cardinal axes.  Var is an LMatrix4.
////////////////////////////////////////////////////////////////////
bool PfmTrans::
dispatch_rotate_xyz(ProgramBase *self, const string &opt, const string &arg, void *var) {
  PfmTrans *base = (PfmTrans *)self;
  return base->ns_dispatch_rotate_xyz(opt, arg, var);
}

////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::ns_dispatch_rotate_xyz
//       Access: Protected
//  Description: Handles -TR, which specifies a rotate transform about
//               the three cardinal axes.  Var is an LMatrix4.
////////////////////////////////////////////////////////////////////
bool PfmTrans::
ns_dispatch_rotate_xyz(const string &opt, const string &arg, void *var) {
  LMatrix4 *transform = (LMatrix4 *)var;

  vector_string words;
  tokenize(arg, words, ",");

  LVecBase3 xyz;

  bool okflag = false;
  if (words.size() == 3) {
    okflag =
      string_to_float(words[0], xyz[0]) &&
      string_to_float(words[1], xyz[1]) &&
      string_to_float(words[2], xyz[2]);
  }

  if (!okflag) {
    nout << "-" << opt
         << " requires three numbers separated by commas.\n";
    return false;
  }

  LMatrix4 mat =
    LMatrix4::rotate_mat(xyz[0], LVector3(1.0, 0.0, 0.0)) *
    LMatrix4::rotate_mat(xyz[1], LVector3(0.0, 1.0, 0.0)) *
    LMatrix4::rotate_mat(xyz[2], LVector3(0.0, 0.0, 1.0));

  *transform = (*transform) * mat;

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::dispatch_rotate_axis
//       Access: Protected, Static
//  Description: Handles -TA, which specifies a rotate transform about
//               an arbitrary axis.  Var is an LMatrix4.
////////////////////////////////////////////////////////////////////
bool PfmTrans::
dispatch_rotate_axis(ProgramBase *self, const string &opt, const string &arg, void *var) {
  PfmTrans *base = (PfmTrans *)self;
  return base->ns_dispatch_rotate_axis(opt, arg, var);
}

////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::ns_dispatch_rotate_axis
//       Access: Protected
//  Description: Handles -TA, which specifies a rotate transform about
//               an arbitrary axis.  Var is an LMatrix4.
////////////////////////////////////////////////////////////////////
bool PfmTrans::
ns_dispatch_rotate_axis(const string &opt, const string &arg, void *var) {
  LMatrix4 *transform = (LMatrix4 *)var;

  vector_string words;
  tokenize(arg, words, ",");

  PN_stdfloat angle;
  LVecBase3 axis;

  bool okflag = false;
  if (words.size() == 4) {
    okflag =
      string_to_float(words[0], angle) &&
      string_to_float(words[1], axis[0]) &&
      string_to_float(words[2], axis[1]) &&
      string_to_float(words[3], axis[2]);
  }

  if (!okflag) {
    nout << "-" << opt
         << " requires four numbers separated by commas.\n";
    return false;
  }

  *transform = (*transform) * LMatrix4::rotate_mat(angle, axis);

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: PfmTrans::dispatch_translate
//       Access: Protected, Static
//  Description: Handles -TT, which specifies a translate transform.
//               Var is an LMatrix4.
////////////////////////////////////////////////////////////////////
bool PfmTrans::
dispatch_translate(const string &opt, const string &arg, void *var) {
  LMatrix4 *transform = (LMatrix4 *)var;

  vector_string words;
  tokenize(arg, words, ",");

  LVector3 trans;

  bool okflag = false;
  if (words.size() == 3) {
    okflag =
      string_to_float(words[0], trans[0]) &&
      string_to_float(words[1], trans[1]) &&
      string_to_float(words[2], trans[2]);
  }

  if (!okflag) {
    nout << "-" << opt
         << " requires three numbers separated by commas.\n";
    return false;
  }

  *transform = (*transform) * LMatrix4::translate_mat(trans);

  return true;
}


int main(int argc, char *argv[]) {
  // A call to pystub() to force libpystub.so to be linked in.
  pystub();

  PfmTrans prog;
  prog.parse_command_line(argc, argv);
  prog.run();
  return 0;
}