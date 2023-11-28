/* prototypes.h --- Shared interfaces used by CGIJACL and JACL
 * (C) 2008 Stuart Allen, distribute and use 
 * according to GNU GPL, see file COPYING for details.
 */

#ifdef GLK
strid_t open_glk_file(glui32 usage, glui32 mode, char *filename);
glui32 glk_get_bin_line_stream(strid_t file_stream, char *buffer, glui32 max_length);
glui32 parse_utf8(unsigned char *buf, glui32 buflen,
				  glui32 *out, glui32 outlen);
void convert_to_utf8(glui32 *text, int len);
glui32 parse_utf8(unsigned char *buf, glui32 buflen, glui32 *out, glui32 outlen);
#else
void update_parameters();
#endif

#ifdef GARGLK
extern char* garglk_fileref_get_name(frefid_t fref);

extern void garglk_set_program_name(const char *name);
extern void garglk_set_program_info(const char *info);
extern void garglk_set_story_name(const char *name);
extern void garglk_set_config(const char *name);
#endif

void default_footer();
void default_header();
int bearing(double x1, double y1, double x2, double y2);
int distance(double x1, double y1, double x2, double y2);
int strcondition(void);
int and_strcondition(void);
int logic_test(int first);
int str_test(int first);
int first_available(int list_number);
int validate(char *string);
int noun_resolve(struct word_type *scope_word, int finding_from, int noun_number);
int get_from_object(struct word_type *scope_word, int noun_number);
int is_direct_child_of_from(int child);
int is_child_of_from(int child);
int verify_from_object(int from_object);
int find_parent(int index);
int scope(int index, char *expected, int restricted);
int object_element_resolve(char *testString);
int execute(char *funcname);
int object_resolve(char object_string[]);
int random_number(void);
void log_access(char *message);
void log_error(char *message, int console);
int parent_of(int parent, int child, int restricted);
int grand_of(int child, int objs_only);
int check_light(int where);
int find_route(int fromRoom, int toRoom, int known);
int exit_function(int return_code);
int	count_resolve(char *testString);
void jacl_set_window(winid_t new_window);
void create_cstring(char *name, char *value);
void create_string(char *name, char *value);
void create_integer(char *name, int value);
void create_cinteger(char *name, int value);
void scripting(void);
void undoing(void);
void walking_thru(void);
void create_paths(char *full_path);
int get_key(void);
char get_character(char *message);
int get_yes_or_no(void);
void get_string(char *string_buffer);
int get_number(int insist, int low, int high);
int save_interaction(char *filename);
int restore_interaction(char *filename);
void write_integer(strid_t stream, int x);
void write_long(strid_t stream, long x);
int  read_integer(strid_t stream);
long read_long(strid_t stream);
void jacl_encrypt (char *string);
void jacl_decrypt (char *string);
void log_message(char *message, int console);
void set_them(int noun_number);
void preparse(void);
void inspect(int object_num);
void add_all(struct word_type *scope_word, int noun_number);
void add_to_list(int noun_number, int resolved_object);
void call_functions(char *base_name);
int build_object_list(struct word_type *scope_word, int noun_number);
long value_of(char *value, int run_time);
long attribute_resolve(char *attribute);
long user_attribute_resolve(char *name);
struct word_type *exact_match(struct word_type *pointer);
struct word_type *object_match(struct word_type *iterator, int noun_number);
struct integer_type *integer_resolve(char *name);
struct integer_type *integer_resolve_indexed(char *name, int index);
struct function_type *function_resolve(char *name);
struct string_type *string_resolve(char *name);
struct string_type *string_resolve_indexed(char *name, int index);
struct string_type *cstring_resolve(char *name);
struct string_type *cstring_resolve_indexed(char *name, int index);
struct cinteger_type *cinteger_resolve(char *name);
struct cinteger_type *cinteger_resolve_indexed(char *name, int index);
int array_length_resolve(char *testString);
int legal_label_check(char *word, int line, int type);
int attribute_test();
char* object_names(int object_index, char *names_buffer);
char* arg_text_of(char *string);
char* arg_text_of_word(int wordnumber);
char* var_text_of_word(int wordnumber);
char* text_of(char *string);
char* text_of_word(int wordnumber);
char* expand_function(char *name);
int* container_resolve(char *container_name);
int condition(void);
int and_condition(void);
void free_from(struct word_type *x);
void word_check(void);
void eachturn(void);
void read_config_file();
void version_info(void);
int jacl_whitespace(int character);
int get_here(void);
char* stripwhite(char *string);
void command_encapsulate(void);
void encapsulate(void);
void jacl_truncate(void);
void parser(void);
void diagnose(void);
void look_around(void);
char* macro_resolve(char *testString);
char* plain_output(int index, int capital);
char* sub_output(int index, int capital);
char* obj_output(int index, int capital);
char* that_output(int index, int capital);
char* sentence_output(int index, int capital);
char* isnt_output(int index, int capital);
char* is_output(int index, int capital);
char* it_output(int index, int capital);
char* doesnt_output(int index, int capital);
char* does_output(int index, int capital);
char* list_output(int index, int capital);
char* long_output(int index);
void terminate(int code);
void set_arguments(char *function_call);
void pop_stack(void);
void push_stack(
#ifdef GLK
	 glsi32          file_pointer
#else
	 long          file_pointer
#endif
);
void pop_proxy(void);
void push_proxy(void);
void write_text(char *string_buffer);
void status_line(void);
void newline(void);
void scroll();
int  save_game(frefid_t saveref);
int  restore_game(frefid_t saveref, int warn);
void save_game_state(void);
void restore_game_state(void);
void add_string();
void add_cstring(char *name, char *value);
void clear_string();
void clear_cstring(char *name);
void add_cinteger(char *name, int value);
void clear_cinteger(char *name);
void restart_game(void);
void read_gamefile(void);
void new_position(double x1, double y1, double bearing, double velocity);
void build_grammar_table(struct word_type *pointer);
void unkvalerr(int line, int wordno);
void totalerrs(int errors);
void unkatterr(int line, int wordno);
void unkfunrun(char *name);
void nofnamerr(int line);
void nongloberr(int line);
void unkkeyerr(int line, int wordno);
void maxatterr(int line, int wordno);
void unkattrun(int wordno);
void badptrrun(char *name, int value);
void badplrrun(int value);
void badparrun(void);
void notintrun(void);
void noproprun(void);
void noproperr(int line);
void noobjerr(int line);
void unkobjerr(int line, int wordno);
void unkobjrun(int wordno);
void unkdirrun(int wordno);
void unkscorun(char *scope);
void unkstrrun(char *variable);
void unkvarrun(char *variable);
void outofmem(void);
void set_defaults(void);
void no_it(void);
void clrscrn();
void more(char* message);
int jpp(void);
int process_file(char *sourceFile1, char *sourceFile2);
char* strip_return(char *string);
char** command_completion();
char* object_generator(char* text, int state);
char* verb_generator(char* text, int state);
void add_word(char *word);
void create_language_constants(void);
int select_next(void);
void jacl_sleep();
