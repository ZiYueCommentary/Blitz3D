#include "MultiLang.h"

// Big5

static constexpr const char* array_bounds_ex = "Blitz陣列索引越界";
static constexpr const char* null_obj_ex = "物件不存在";
static constexpr const char* bad_data_type = "錯誤的資料型別！資料型別不應是浮點值、字串或整型";
static constexpr const char* out_of_data = "資料不足";
static constexpr const char* stats_strings = "啟用的字串：{0}";
static constexpr const char* stats_objects = "啟用的物件：{0}";
static constexpr const char* stats_unreleased = "未釋放的物件：{0}";
static constexpr const char* sound_not_exist = "聲音不存在";
static constexpr const char* bank_not_exist = "bbBank不存在";
static constexpr const char* offset_out_of_range = "位置超出範圍";
static constexpr const char* graphics_not_set = "3D畫面模式未設定";
static constexpr const char* texture_not_exist = "紋理不存在";
static constexpr const char* brush_not_exist = "Brush不存在";
static constexpr const char* entity_not_exist = "實體不存在";
static constexpr const char* parent_entity_not_exist = "父實體不存在";
static constexpr const char* entity_not_mesh = "實體不是Mesh";
static constexpr const char* entity_not_object = "實體不是物件";
static constexpr const char* collision_out_of_range = "碰撞索引超出範圍";
static constexpr const char* entity_not_camera = "實體不是相機";
static constexpr const char* entity_not_light = "實體不是燈光";
static constexpr const char* entity_not_model = "實體不是模型";
static constexpr const char* entity_not_sprite = "實體不是Sprite";
static constexpr const char* entity_not_md2_model = "實體不是MD2模型";
static constexpr const char* entity_not_bsp_model = "實體不是BSP模型";
static constexpr const char* entity_not_terrain = "實體不是地形";
static constexpr const char* illegal_number_segments = "段數非法";
static constexpr const char* vertex_out_of_range = "頂點索引超出範圍";
static constexpr const char* texture_coordinate_out_of_range = "紋理座標設定超出範圍";
static constexpr const char* illegal_texture_frames = "非法紋理幀數";
static constexpr const char* mesh_cannot_add_to_self = "無法將Mesh新增到其本身";
static constexpr const char* surface_out_of_range = "表面索引超出範圍";
static constexpr const char* illegal_terrain_size = "非法地形大小";
static constexpr const char* unable_load_heightmap = "無法載入高度圖";
static constexpr const char* terrain_must_be_square = "地表必須為正方形";
static constexpr const char* listener_already_created = "已建立監聽器";
static constexpr const char* no_listener_created = "未建立監聽器";
static constexpr const char* entity_cannot_parented_itself = "實體不能為其自身的父實體";
static constexpr const char* entity_no_animations = "實體無動畫";
static constexpr const char* entity_not_model_or_camera = "實體不是模型或相機";
static constexpr const char* entitytype_id_range = "EntityType ID必須為0至999之間的數字";
static constexpr const char* unable_create_gxscene_instance = "無法建立gxScene例項";
static constexpr const char* file_not_exist = "檔案不存在";
static constexpr const char* directory_not_exist = "資料夾不存在";
static constexpr const char* image_not_exist = "影象不存在";
static constexpr const char* image_frame_out_of_range = "影象幀超出範圍";
static constexpr const char* font_not_exist = "字型不存在";
static constexpr const char* buffer_not_exist = "緩衝區不存在";
static constexpr const char* illegal_graphics_driver_index = "非法顯示驅動索引";
static constexpr const char* illegal_graphics_mode_index = "非法顯示模式索引";
static constexpr const char* unable_create_gxgraphics_instance = "無法建立gxGraphics例項";
static constexpr const char* illegal_graphics_mode = "非法顯示模式，顯示模式為0到7之間的值。";
static constexpr const char* illegal_graphics3d_mode = "非法3D顯示模式，顯示模式為0到7之間的值。";
static constexpr const char* unable_close_gxgraphics_instance = "無法關閉gxGraphics例項";
static constexpr const wchar_t* runtime_error = L"執行時錯誤！";
static constexpr const char* illegal_frame_count = "非法幀數";
static constexpr const char* illegal_first_frame = "非法第一幀";
static constexpr const char* not_enough_frames_bitmap = "點陣圖幀數不足";
static constexpr const char* udp_stream_not_exist = "UDP流不存在";
static constexpr const char* tcp_stream_not_exist = "TCP流不存在";
static constexpr const char* tcp_server_not_exist = "TCP伺服器不存在";
static constexpr const char* host_out_of_range = "主機索引超出範圍";
static constexpr const char* stream_not_exist = "流不存在";
static constexpr const char* readbyte_invalid_byte = "ReadByte無法讀取有效位元組\n請確保讀取的檔案尚未損壞";
static constexpr const char* readshort_invalid_short = "ReadShort無法讀取有效短整型\n請確保讀取的檔案尚未損壞";
static constexpr const char* readint_invalid_int = "ReadInt無法讀取有效整型\n請確保讀取的檔案尚未損壞";
static constexpr const char* readfloat_invalid_float = "ReadFloat無法讀取有效浮點值\n請確保讀取的檔案尚未損壞";
static constexpr const char* readstring_invalid_string = "ReadString無法讀取有效字串\n請確保讀取的檔案尚未損壞";
static constexpr const char* illegal_buffer_size = "非法緩衝區大小";
static constexpr const char* string_parameter_positive = "引數必須為正";
static constexpr const char* string_parameter_greater = "引數必須大於0";
static constexpr const char* userlib_not_found = "使用者庫未找到：{0}";
static constexpr const char* userlib_function_not_found = "使用者庫函式未找到：{0}";
static constexpr const wchar_t* integer_divide_zero = L"整數除零";
static constexpr const char* memory_access_violation = "記憶體訪問衝突！\n程式試圖讀取或寫入受保護的記憶體地址。";
static constexpr const wchar_t* illegal_instruction = L"非法指令。\n程式試圖執行無效的CPU指令";
static constexpr const wchar_t* stack_overflow = L"堆疊溢位！\n請確保程式內沒有遞迴現象";
static constexpr const wchar_t* integer_overflow = L"整數溢位！\n請確保整數的值不超過2147483647";
static constexpr const wchar_t* float_overflow = L"浮點溢位！\n請確保浮點的值不超過3.40282347e+38F";
static constexpr const wchar_t* float_divide_zero = L"浮點除零";
static constexpr const wchar_t* unknown_runtime_exception = L"未知執行時錯誤";
static constexpr const char* unable_run_module = "無法啟動Blitz Basic模組";
static constexpr const char* cant_find_symbol = "找不到符號：{0}";
static constexpr const char* unknown_exception_thrown = "未知/非標準異常丟擲！";
static constexpr const char* startup_error = "啟動錯誤：{0}";
static constexpr const char* array_index_out_of_bounds = "陣列索引超出範圍";
static constexpr const char* unable_start_program = "無法啟動程式！無法啟動所需模組";
static constexpr const char* reserved_pseudo_namespace = "保留的偽名稱空間";
static constexpr const char* expect_identifier = "\".\"後應為識別符號";
static constexpr const char* expect_string_afrer_directive = "lib指令後應為字串";
static constexpr const char* unknown_decl_directive = "未知庫指令";
static constexpr const char* function_decl_without_directive = "無lib的函式庫";
static constexpr const char* duplicate_identifier = "重複識別符號";
static constexpr const char* expect_left_bracket_after_function_identifier = "函式名後缺少\"(\"";
static constexpr const char* expect_right_bracket_after_function_identifier = "函式引數後缺少\")\"";
static constexpr const char* expect_identifier_or_string_after_alias = "別名後應為識別符號或字串";
static constexpr const char* unable_open_linker_dll = "無法開啟linker.dll";
static constexpr const char* error_in_linker_dll = "linker.dll出錯";
static constexpr const char* unable_open_runtime_dll = "無法開啟runtime.dll";
static constexpr const char* error_in_runtime_dll = "runtime.dll出錯";
static constexpr const char* library_version_error = "庫版本出錯";
static constexpr const char* immediate_value_cannot_by_label = "立即值不能透過標籤";
static constexpr const char* operand_must_be_immediate = "運算元必須為立即數";
static constexpr const char* unrecognized_instruction = "未識別指令";
static constexpr const char* illegal_addressing_mode = "非法尋地址方式";
static constexpr const char* operand_error = "運算元誤差";
static constexpr const char* duplicate_label = "重複標籤";
static constexpr const char* missing_close_quote = "缺失後引號";
static constexpr const char* expect_comma = "缺少\",\"";
static constexpr const char* too_many_operands = "運算元過多";
static constexpr const char* expression_must_be_constant = "表示式必須為常數";
static constexpr const char* constants_must_initialized = "常數必須被初始化";
static constexpr const char* duplicate_variable_name = "重複變數名";
static constexpr const char* undefined_label = "未定義標籤";
static constexpr const char* data_expression_must_be_constant = "資料表示式必須為常數";
static constexpr const char* blitz_array_sizes_must_be_constant = "Blitz陣列大小必須為常數";
static constexpr const char* blitz_array_sizes_must_not_negative = "Blitz陣列大小不能為負數";
static constexpr const char* illegal_type_conversion = "非法型別轉換（{0} -> {1}）";
static constexpr const char* too_many_parameters = "引數過多";
static constexpr const char* not_enough_parameters = "引數過少";
static constexpr const char* function_not_found = "函式\"{0}\"未找到";
static constexpr const char* incorrect_function_return_type = "錯誤函式返回值";
static constexpr const char* cant_convert_null_to_int = "不能將null轉為整數";
static constexpr const char* cant_convert_null_to_float = "不能將null轉為浮點值";
static constexpr const char* cant_convert_null_to_string = "不能將null轉為字串";
static constexpr const char* illegal_operator_for_type = "非法型別運算子";
static constexpr const char* arithmetic_operator_custom_type = "算術運算子不能用於自定型別物件";
static constexpr const char* operator_cannot_applied_to_strings = "運算子不能用於字串";
static constexpr const char* division_by_zero = "數字除零";
static constexpr const char* custom_type_not_found = "自定型別名未找到";
static constexpr const char* type_is_not_custom_type = "型別不是自定型別";
static constexpr const char* after_cannot_used_on_null = "\"After\"不能用於\"Null\"";
static constexpr const char* after_must_used_with_custom_type = "\"After\"必須和自定型別物件使用";
static constexpr const char* before_cannot_used_with_null = "\"Before\"不能用於\"Null\"";
static constexpr const char* before_must_used_with_custom_type = "\"Before\"必須和自定型別物件使用";
static constexpr const char* objecthandle_must_used_with_object = "\"ObjectHandle\"必須和物件使用";
static constexpr const char* internal_compiler_error = "編譯器內部錯誤！";
static constexpr const char* type_not_found = "型別\"{0}\"未找到";
static constexpr const char* error_in_operand = "運算元錯誤";
static constexpr const char* illegal_operand_size = "非法運算元大小";
static constexpr const char* register_must_be_32_bit = "暫存器必須為32位";
static constexpr const char* next_without_for = "\"Next\"缺少\"For\"";
static constexpr const char* wend_without_while = "\"Wend\"缺少\"While\"";
static constexpr const char* else_without_if = "\"Else\"缺少\"If\"";
static constexpr const char* endif_without_if = "\"Endif\"缺少\"If\"";
static constexpr const char* end_function_without_function = "\"End Function\"缺少\"Function\"";
static constexpr const char* until_without_repeat = "\"Until\"缺少\"Repeat\"";
static constexpr const char* forever_without_repeat = "\"Forever\"缺少\"Repeat\"";
static constexpr const char* case_without_select = "\"Case\"缺少\"Select\"";
static constexpr const char* end_select_without_select = "\"End Select\"缺少\"Select\"";
static constexpr const char* expecting = "缺少{0}";
static constexpr const char* identifier = "識別符號";
static constexpr const char* include_filename = "匯入檔名";
static constexpr const char* unable_open_include_file = "無法開啟匯入檔案";
static constexpr const char* end_of_file = "檔案尾部（EOF）";
static constexpr const char* mismatched_brackets = "括號不匹配";
static constexpr const char* variable_assignment = "變數賦值";
static constexpr const char* until_or_forever = "\"Until\"或\"Forever\"";
static constexpr const char* expression_sequence = "表示式序列";
static constexpr const char* case_default_or_end_select = "\"Case\"、\"Default\"或\"End Select\"";
static constexpr const char* before_or_after = "\"Before\"或\"After\"";
static constexpr const char* data_can_only_appear_in_main = "\"Data\"只能在主程式出現";
static constexpr const char* type_can_only_appear_in_main = "\"Type\"只能在主程式出現";
static constexpr const char* const_can_only_appear_in_main = "\"Const\"只能在主程式出現";
static constexpr const char* function_can_only_appear_in_main = "\"Function\"只能在主程式出現";
static constexpr const char* global_can_only_appear_in_main = "\"Global\"只能在主程式出現";
static constexpr const char* blitz_arrays_may_not_be_constant = "Blitz陣列不能為常量";
static constexpr const char* cant_have_zero_dimension_array = "不能有零維陣列";
static constexpr const char* field_or_end_type = "\"Field\"或\"End Type\"";
static constexpr const char* expression = "表示式";
static constexpr const char* undefined_label_check = "未宣告標籤\"{0}\"";
static constexpr const char* array_not_found_in_main = "在主程式中找不到陣列";
static constexpr const char* constants_can_not_assigned_to = "不能將常量分配給";
static constexpr const char* blitz_arrays_can_not_assigned_to = "不能將Blitz陣列分配給";
static constexpr const char* gosub_may_not_used_inside_function = "\"Gosub\"不能在函式內使用";
static constexpr const char* break_must_appear_inside_loop = "break必須在迴圈中出現";
static constexpr const char* index_variable_can_not_constant = "索引變數不能為常量";
static constexpr const char* index_variable_must_integer_or_real = "索引變數必須為整數或實數";
static constexpr const char* step_value_must_constant = "步進值必須為常量";
static constexpr const char* index_variable_is_not_newtype = "索引變數不是新型別";
static constexpr const char* type_name_not_found = "型別名未找到";
static constexpr const char* type_mismatch = "型別不匹配";
static constexpr const char* main_cannot_return_value = "主程式不能返回值";
static constexpr const char* cant_delete_non_newtype = "不能刪除非新型別";
static constexpr const char* specified_name_is_not_newtype_name = "指定名稱不是新型別名";
static constexpr const char* illegal_expression_type = "非法表示式型別";
static constexpr const char* objects_types_are_different = "對量型別不同";
static constexpr const char* select_cannot_used_with_objects = "Select不能用於物件";
static constexpr const char* constants_can_not_modified = "常量不可被修改";
static constexpr const char* data_can_not_read_into_object = "無法將資料讀入Object";
static constexpr const char* variable_type_mismatch = "變數型別不匹配";
static constexpr const char* identifier_not_used_like_this = "識別符號\"{0}\"不允許如此使用";
static constexpr const char* array_not_found = "陣列未找到";
static constexpr const char* array_type_mismatch = "陣列型別不匹配";
static constexpr const char* incorrect_number_of_dimensions = "錯誤維度數字";
static constexpr const char* variable_must_be_type = "變數必須為型別";
static constexpr const char* type_field_not_found = "型別資料未找到";
static constexpr const char* variable_must_a_blitz_array = "變數必須為Blitz陣列";
static constexpr const char* incorrect_number_of_subscripts = "下標數出錯";
static constexpr const char* blitz_array_subscript_out_of_range = "Blitz陣列下標超出範圍";

static constexpr const wchar_t* runtime_message = L"執行時訊息";
static constexpr const char* current_language = "當前語言：簡體中文";
static constexpr const char* debugger_locals = "本地";
static constexpr const char* debugger_globals = "全域性";
static constexpr const char* debugger_consts = "常量";
static constexpr const char* debugger_unknown = "<未知>";
static constexpr const char* debugger_cant_access_appdata = "無法訪問AppData資料夾！配置檔案無法讀取。\nIDE將使用預設值";
static constexpr const char* debugger_cant_create_folder = "無法為配置檔案建立資料夾！\nIDE將使用預設值";
static constexpr const char* debugger_empty_ini = "blitzide.ini為空！\n將使用預設設定";
static constexpr char credits[] =
"程式設計及設計：Mark Sibly\r\n"
"專案續命：juanjp600、Saalvage、VaneBrain、AestheticalZ和ZiYueCommentary\r\n"
".INI載入器使用\"IniPP\"的修改版本，原版由Matthias C. M. Troffaes製作\r\n"
"文件：Mark Sibly、Simon Harrison、Paul Gerfen、Shane Monroe和Blitz文件撰寫團隊\r\n"
"測試及支援：James Boyd、Simon Armstrong和Blitz開發團隊\r\n"
"影象載入器：Floris van den berg製作的FreeImage";
static constexpr const char* title_release = " - 釋出版本";
static constexpr const char* program_ended = "程式已結束";
static constexpr const char* bb_err = "Blitz錯誤";
static constexpr const char* init_err = "啟動BlitzIDE時出錯。請檢查\"bin\"資料夾內是否有ide.exe。";
static constexpr const char* blitz3d_message = "Blitz3D訊息";
static constexpr const char* created_with_beta = "使用Blitz3D Beta V{0}.{1}製作";
static constexpr const char* invalid_macro_name = "無效宏名稱：{0}";
static constexpr const char* redefinition_of_macro = "重複宏名稱：{0}";
static constexpr const char* error_evaluating_condition = "錯誤宏運算語句：{0}";
static constexpr const char* elseif_without_if = "#elseif_缺少#if_";
static constexpr const char* else_without_if_macro = "#else_缺少#if_";
static constexpr const char* macro_exceeded = "宏深度達到限制";
static constexpr const char* unsupported_operator = "無效運算子：{0}";
static constexpr const char* expression_evaluation_resulted_in_no_value = "表示式沒有結果";
static constexpr const char* invalid_macro_string = "不是字符串";