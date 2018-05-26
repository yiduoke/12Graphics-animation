/*========== my_main.c ==========

  This is the only file you need to modify in order
  to get a working mdl project (for now).

  my_main.c will serve as the interpreter for mdl.
  When an mdl script goes through a lexer and parser,
  the resulting operations will be in the array op[].

  Your job is to go through each entry in op and perform
  the required action from the list below:

  push: push a new origin matrix onto the origin stack
  pop: remove the top matrix on the origin stack

  move/scale/rotate: create a transformation matrix
                     based on the provided values, then
                     multiply the current top of the
                     origins stack by it.

  box/sphere/torus: create a solid object based on the
                    provided values. Store that in a
                    temporary matrix, multiply it by the
                    current top of the origins stack, then
                    call draw_polygons.

  line: create a line based on the provided values. Stores
        that in a temporary matrix, multiply it by the
        current top of the origins stack, then call draw_lines.

  save: call save_extension with the provided filename

  display: view the image live
  =========================*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "parser.h"
#include "symtab.h"
#include "y.tab.h"

#include "matrix.h"
#include "ml6.h"
#include "display.h"
#include "draw.h"
#include "stack.h"
#include "gmath.h"


/*======== void first_pass() ==========
  Inputs:
  Returns:

  Checks the op array for any animation commands
  (frames, basename, vary)

  Should set num_frames and basename if the frames
  or basename commands are present

  If vary is found, but frames is not, the entire
  program should exit.

  If frames is found, but basename is not, set name
  to some default value, and print out a message
  with the name being used.
  ====================*/
void first_pass() {
  //in order to use name and num_frames throughout
  //they must be extern variables
  extern int num_frames;
  extern char name[128];
  
  int has_frames = 0;
 	int has_vary = 0;
 	int has_name = 0;
	
	int i;
	for (i = 0; i < lastop; i++){
		switch(op[i].opcode){
			case FRAMES:
				if (has_frames){
					printf("exiting program; multiple frames\n");
					exit(0);
				}
				has_frames = 1;
				num_frames = op[i].op.frames.num_frames;
				break;
			case BASENAME:
				if (has_name){
					printf("exiting program; multiple base names\n");
					exit(0);
				}
				has_name = 1;
				strcpy(name, op[i].op.basename.p->name);
				break;
			case VARY:
				has_vary = 1;
				break;
		}
	}
	
	if (has_vary && !has_frames){
		printf("has vary but no frames\n");
		exit(0);
	}
	if (has_frames && !has_name){
		strcpy(name, "tudous");
		printf("no basename found, just used tudous\n");
	}
}

/*======== struct vary_node ** second_pass() ==========
  Inputs:
  Returns: An array of vary_node linked lists

  In order to set the knobs for animation, we need to keep
  a seaprate value for each knob for each frame. We can do
  this by using an array of linked lists. Each array index
  will correspond to a frame (eg. knobs[0] would be the first
  frame, knobs[2] would be the 3rd frame and so on).

  Each index should contain a linked list of vary_nodes, each
  node contains a knob name, a value, and a pointer to the
  next node.

  Go through the opcode array, and when you find vary, go
  from knobs[0] to knobs[frames-1] and add (or modify) the
  vary_node corresponding to the given knob with the
  appropirate value.
  ====================*/
struct vary_node ** second_pass() {
	int i, j;
	struct vary_node** knob_array = calloc(num_frames, sizeof(struct vary_node*));
	
	for (i = 0; i < lastop; i++){
		if (op[i].opcode == VARY){
			double start_frame = op[i].op.vary.start_frame,
				end_frame = op[i].op.vary.end_frame,
				start_val = op[i].op.vary.start_val,
        end_val = op[i].op.vary.end_val;
      char *knob_name = op[i].op.vary.p->name;
        
      double delta = (end_val - start_val) / (end_frame - start_frame);
				for (j = 0; j < end_frame - start_frame; j++){
          struct vary_node * new_knob = malloc(sizeof(struct vary_node*));
          strcpy(new_knob->name, knob_name);
          new_knob->value = start_val + j * delta;
          if (!knob_array[j]){
            new_knob->next = NULL;
          }
          else{
            new_knob->next = knob_array[j];
          }
          knob_array[j] = new_knob;
					//https://github.com/yiduoke/06Sys/blob/master/linkedList.c
				}
		}
	}
  return knob_array;
}

/*======== void print_knobs() ==========
Inputs:
Returns:

Goes through symtab and display all the knobs and their
currnt values
====================*/
void print_knobs() {
  int i;

  printf( "ID\tNAME\t\tTYPE\t\tVALUE\n" );
  for ( i=0; i < lastsym; i++ ) {

    if ( symtab[i].type == SYM_VALUE ) {
      printf( "%d\t%s\t\t", i, symtab[i].name );

      printf( "SYM_VALUE\t");
      printf( "%6.2f\n", symtab[i].s.value);
    }
  }
}

/*======== void my_main() ==========
  Inputs:
  Returns:

  This is the main engine of the interpreter, it should
  handle most of the commadns in mdl.

  If frames is not present in the source (and therefore
  num_frames is 1, then process_knobs should be called.

  If frames is present, the enitre op array must be
  applied frames time. At the end of each frame iteration
  save the current screen to a file named the
  provided basename plus a numeric string such that the
  files will be listed in order, then clear the screen and
  reset any other data structures that need it.

  Important note: you cannot just name your files in
  regular sequence, like pic0, pic1, pic2, pic3... if that
  is done, then pic1, pic10, pic11... will come before pic2
  and so on. In order to keep things clear, add leading 0s
  to the numeric portion of the name. If you use sprintf,
  you can use "%0xd" for this purpose. It will add at most
  x 0s in front of a number, if needed, so if used correctly,
  and x = 4, you would get numbers like 0001, 0002, 0011,
  0487
  ====================*/
void my_main() {
	print_symtab();
  int i;
  struct matrix *tmp;
  struct stack *systems;
  screen t;
  zbuffer zb;
  color g;
  g.red = 0;
  g.green = 0;
  g.blue = 0;
  double step_3d = 20;
  double theta;
  double knob_value, xval, yval, zval;

  //Lighting values here for easy access
  color ambient;
  double light[2][3];
  double view[3];
  double areflect[3];
  double dreflect[3];
  double sreflect[3];

  ambient.red = 50;
  ambient.green = 50;
  ambient.blue = 50;

  light[LOCATION][0] = 0.5;
  light[LOCATION][1] = 0.75;
  light[LOCATION][2] = 1;

  light[COLOR][RED] = 0;
  light[COLOR][GREEN] = 255;
  light[COLOR][BLUE] = 255;

  view[0] = 0;
  view[1] = 0;
  view[2] = 1;

  areflect[RED] = 0.1;
  areflect[GREEN] = 0.1;
  areflect[BLUE] = 0.1;

  dreflect[RED] = 0.5;
  dreflect[GREEN] = 0.5;
  dreflect[BLUE] = 0.5;

  sreflect[RED] = 0.5;
  sreflect[GREEN] = 0.5;
  sreflect[BLUE] = 0.5;

  systems = new_stack();
  tmp = new_matrix(4, 1000);
  clear_screen( t );
  clear_zbuffer(zb);
  
  struct vary_node** knobs = second_pass();
  
  if (num_frames > 1){
  	int current_frame;
  	for (current_frame = 0; current_frame < num_frames; current_frame++){
  		printf("frame %d\n", current_frame);
  		struct vary_node * current_node = knobs[current_frame];
  		
  		while(current_node){
  			set_value(lookup_symbol(current_node -> name), current_node -> value);
  			current_node = current_node -> next;
  		}
  		
  		  for (i=0;i<lastop;i++) {
				//printf("%d: ",i);
				switch (op[i].opcode)
				  {
				  case SET:
				  	set_value(lookup_symbol(op[i].op.set.p->name), op[i].op.set.val);
				  	break;
				  case SETKNOBS:
				  	current_node = knobs[current_frame];
				  	int current_symbol;
				  	for (current_symbol = 0; current_symbol < lastsym; current_symbol++){
				  		if (symtab[current_symbol].type == SYM_VALUE){
				  			symtab[current_symbol].s.value = op[i].op.setknobs.value;
				  		}
				  	}
				  case SPHERE:
				    /* printf("Sphere: %6.2f %6.2f %6.2f r=%6.2f", */
				    /* 	 op[i].op.sphere.d[0],op[i].op.sphere.d[1], */
				    /* 	 op[i].op.sphere.d[2], */
				    /* 	 op[i].op.sphere.r); */
				    if (op[i].op.sphere.constants != NULL)
				      {
				        //printf("\tconstants: %s",op[i].op.sphere.constants->name);
				      }
				    if (op[i].op.sphere.cs != NULL)
				      {
				        //printf("\tcs: %s",op[i].op.sphere.cs->name);
				      }
				    add_sphere(tmp, op[i].op.sphere.d[0],
				               op[i].op.sphere.d[1],
				               op[i].op.sphere.d[2],
				               op[i].op.sphere.r, step_3d);
				    matrix_mult( peek(systems), tmp );
				    draw_polygons(tmp, t, zb, view, light, ambient,
				                  areflect, dreflect, sreflect);
				    tmp->lastcol = 0;
				    break;
				  case TORUS:
				    /* printf("Torus: %6.2f %6.2f %6.2f r0=%6.2f r1=%6.2f", */
				    /* 	 op[i].op.torus.d[0],op[i].op.torus.d[1], */
				    /* 	 op[i].op.torus.d[2], */
				    /* 	 op[i].op.torus.r0,op[i].op.torus.r1); */
				    if (op[i].op.torus.constants != NULL)
				      {
				        //printf("\tconstants: %s",op[i].op.torus.constants->name);
				      }
				    if (op[i].op.torus.cs != NULL)
				      {
				        //printf("\tcs: %s",op[i].op.torus.cs->name);
				      }
				    add_torus(tmp,
				              op[i].op.torus.d[0],
				              op[i].op.torus.d[1],
				              op[i].op.torus.d[2],
				              op[i].op.torus.r0,op[i].op.torus.r1, step_3d);
				    matrix_mult( peek(systems), tmp );
				    draw_polygons(tmp, t, zb, view, light, ambient,
				                  areflect, dreflect, sreflect);
				    tmp->lastcol = 0;
				    break;
				  case BOX:
				    /* printf("Box: d0: %6.2f %6.2f %6.2f d1: %6.2f %6.2f %6.2f", */
				    /* 	 op[i].op.box.d0[0],op[i].op.box.d0[1], */
				    /* 	 op[i].op.box.d0[2], */
				    /* 	 op[i].op.box.d1[0],op[i].op.box.d1[1], */
				    /* 	 op[i].op.box.d1[2]); */
				    if (op[i].op.box.constants != NULL)
				      {
				        //printf("\tconstants: %s",op[i].op.box.constants->name);
				      }
				    if (op[i].op.box.cs != NULL)
				      {
				        //printf("\tcs: %s",op[i].op.box.cs->name);
				      }
				    add_box(tmp,
				            op[i].op.box.d0[0],op[i].op.box.d0[1],
				            op[i].op.box.d0[2],
				            op[i].op.box.d1[0],op[i].op.box.d1[1],
				            op[i].op.box.d1[2]);
				    matrix_mult( peek(systems), tmp );
				    draw_polygons(tmp, t, zb, view, light, ambient,
				                  areflect, dreflect, sreflect);
				    tmp->lastcol = 0;
				    break;
				  case LINE:
				    /* printf("Line: from: %6.2f %6.2f %6.2f to: %6.2f %6.2f %6.2f",*/
				    /* 	 op[i].op.line.p0[0],op[i].op.line.p0[1], */
				    /* 	 op[i].op.line.p0[1], */
				    /* 	 op[i].op.line.p1[0],op[i].op.line.p1[1], */
				    /* 	 op[i].op.line.p1[1]); */
				    if (op[i].op.line.constants != NULL)
				      {
				        //printf("\n\tConstants: %s",op[i].op.line.constants->name);
				      }
				    if (op[i].op.line.cs0 != NULL)
				      {
				        //printf("\n\tCS0: %s",op[i].op.line.cs0->name);
				      }
				    if (op[i].op.line.cs1 != NULL)
				      {
				        //printf("\n\tCS1: %s",op[i].op.line.cs1->name);
				      }
				    add_edge(tmp,
				             op[i].op.line.p0[0],op[i].op.line.p0[1],
				             op[i].op.line.p0[2],
				             op[i].op.line.p1[0],op[i].op.line.p1[1],
				             op[i].op.line.p1[2]);
				    matrix_mult( peek(systems), tmp );
				    draw_lines(tmp, t, zb, g);
				    tmp->lastcol = 0;
				    break;

				  case MOVE:
				  	knob_value = 1;
				  	if (op[i].op.move.p){
				        printf("\tknob: %s",op[i].op.move.p->name);
				        knob_value = lookup_symbol(op[i].op.move.p->name)->s.value;
				    }
				    xval = op[i].op.move.d[0] * knob_value;
				    yval = op[i].op.move.d[1] * knob_value;
				    zval = op[i].op.move.d[2] * knob_value;
				    printf("Move: %6.2f %6.2f %6.2f",
				           xval, yval, zval);
				    tmp = make_translate( xval, yval, zval );
				    matrix_mult(peek(systems), tmp);
				    copy_matrix(tmp, peek(systems));
				    tmp->lastcol = 0;
				    break;
				  case SCALE:
				  	knob_value = 1;
				  	if (op[i].op.scale.p){
				        printf("\tknob: %s",op[i].op.move.p->name);
				        knob_value = lookup_symbol(op[i].op.move.p->name)->s.value;
				    xval = op[i].op.scale.d[0] * knob_value;
				    yval = op[i].op.scale.d[1] * knob_value;
				    zval = op[i].op.scale.d[2] * knob_value;
				    printf("Scale: %6.2f %6.2f %6.2f",
				           xval, yval, zval);
				    tmp = make_scale( xval, yval, zval );
				    matrix_mult(peek(systems), tmp);
				    copy_matrix(tmp, peek(systems));
				    tmp->lastcol = 0;
				    break;
				  case ROTATE:
				  	knob_value = 1;
				  	if (op[i].op.rotate.p){
				        printf("\tknob: %s",op[i].op.move.p->name);
				        knob_value = lookup_symbol(op[i].op.move.p->name)->s.value;
				    xval = op[i].op.rotate.axis;
				    theta = op[i].op.rotate.degrees * (M_PI / 180) * knob_value;
				    printf("Rotate: axis: %6.2f degrees: %6.2f",
				           xval, theta);
				    if (op[i].op.rotate.axis == 0 )
				      tmp = make_rotX( theta );
				    else if (op[i].op.rotate.axis == 1 )
				      tmp = make_rotY( theta );
				    else
				      tmp = make_rotZ( theta );

				    matrix_mult(peek(systems), tmp);
				    copy_matrix(tmp, peek(systems));
				    tmp->lastcol = 0;
				    break;
				  case PUSH:
				    //printf("Push");
				    push(systems);
				    break;
				  case POP:
				    //printf("Pop");
				    pop(systems);
				    break;
				  case SAVE:
				    //printf("Save: %s",op[i].op.save.p->name);
				    save_extension(t, op[i].op.save.p->name);
				    break;
				  case DISPLAY:
				    //printf("Display");
				    display(t);
				    break;
				  } //end opcode switch
				printf("\n");
			}//end operation loop
			
			char pic_name[256];
			sprintf (pic_name, "%s%03d", name, num_frames);
			save_extension(t, pic_name);
			systems = new_stack();
			clear_screen(t);
  	}
  }
}
}
}
