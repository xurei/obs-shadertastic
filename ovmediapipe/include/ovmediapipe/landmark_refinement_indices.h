// Copyright(C) 2022-2023 Intel Corporation
// SPDX - License - Identifier: Apache - 2.0
#pragma once

const size_t nRefinedLandmarks = 478;
const size_t lips_refined_region_num_points = 80;
const size_t left_eye_refined_region_num_points = 71;
const size_t right_eye_refined_region_num_points = 71;
const size_t left_iris_refined_region_num_points = 5;
const size_t right_iris_refined_region_num_points = 5;

const unsigned short lips_refinement_indices[lips_refined_region_num_points] =
{
    61, 146, 91, 181, 84, 17, 314, 405, 321, 375, 291, 185, 40, 39, 37, 0,
    267, 269, 270, 409, 78, 95, 88, 178, 87, 14, 317, 402, 318, 324, 308, 191,
    80, 81, 82, 13, 312, 311, 310, 415, 76, 77, 90, 180, 85, 16, 315, 404,
    320, 307, 306, 184, 74, 73, 72, 11, 302, 303, 304, 408, 62, 96, 89, 179,
    86, 15, 316, 403, 319, 325, 292, 183, 42, 41, 38, 12, 268, 271, 272, 407,
};

const unsigned short left_eye_refinement_indices[left_eye_refined_region_num_points] =
{
    33, 7, 163, 144, 145, 153, 154, 155, 133, 246, 161, 160, 159, 158, 157, 173,
    130, 25, 110, 24, 23, 22, 26, 112, 243, 247, 30, 29, 27, 28, 56, 190,
    226, 31, 228, 229, 230, 231, 232, 233, 244, 113, 225, 224, 223, 222, 221, 189,
    35, 124, 46, 53, 52, 65, 143, 111, 117, 118, 119, 120, 121, 128, 245, 156,
    70, 63, 105, 66, 107, 55, 193,
};

const unsigned short right_eye_refinement_indices[right_eye_refined_region_num_points] =
{
    263, 249, 390, 373, 374, 380, 381, 382, 362, 466, 388, 387, 386, 385, 384, 398,
    359, 255, 339, 254, 253, 252, 256, 341, 463, 467, 260, 259, 257, 258, 286, 414,
    446, 261, 448, 449, 450, 451, 452, 453, 464, 342, 445, 444, 443, 442, 441, 413,
    265, 353, 276, 283, 282, 295, 372, 340, 346, 347, 348, 349, 350, 357, 465, 383,
    300, 293, 334, 296, 336, 285, 417,
};

const unsigned short left_iris_refinement_indices[left_iris_refined_region_num_points] =
{
    468, 469, 470, 471, 472,
};

const unsigned short right_iris_refinement_indices[right_iris_refined_region_num_points] =
{
    473, 474, 475, 476, 477,
};

const unsigned short left_iris_z_avg_indices[16] =
{
    33, 7, 163, 144, 145, 153, 154, 155, 133, 246, 161, 160, 159, 158, 157, 173
};

const unsigned short right_iris_z_avg_indices[16] =
{
    263, 249, 390, 373, 374, 380, 381, 382, 362, 466, 388, 387, 386, 385, 384, 398
};


// A list of indices that are not part of the lips or eye indices lists defined above.
const unsigned short not_lips_eyes_indices[310] =
{
    1, 2, 3, 4, 5, 6, 8, 9, 10, 18, 19, 20, 21, 32, 34, 36,
    43, 44, 45, 47, 48, 49, 50, 51, 54, 57, 58, 59, 60, 64, 67, 68,
    69, 71, 75, 79, 83, 92, 93, 94, 97, 98, 99, 100, 101, 102, 103, 104,
    106, 108, 109, 114, 115, 116, 122, 123, 125, 126, 127, 129, 131, 132, 134, 135,
    136, 137, 138, 139, 140, 141, 142, 147, 148, 149, 150, 151, 152, 162, 164, 165,
    166, 167, 168, 169, 170, 171, 172, 174, 175, 176, 177, 182, 186, 187, 188, 192,
    194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 227, 234, 235, 236, 237,
    238, 239, 240, 241, 242, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258,
    259, 260, 261, 262, 263, 264, 265, 266, 273, 274, 275, 276, 277, 278, 279, 280,
    281, 282, 283, 284, 286, 287, 288, 289, 290, 294, 295, 297, 298, 299, 301, 305,
    309, 313, 322, 323, 326, 327, 328, 329, 330, 331, 332, 333, 335, 337, 338, 339,
    340, 341, 342, 343, 344, 345, 346, 347, 348, 349, 350, 351, 352, 353, 354, 355,
    356, 357, 358, 359, 360, 361, 362, 363, 364, 365, 366, 367, 368, 369, 370, 371,
    372, 373, 374, 376, 377, 378, 379, 380, 381, 382, 383, 384, 385, 386, 387, 388,
    389, 390, 391, 392, 393, 394, 395, 396, 397, 398, 399, 400, 401, 406, 410, 411,
    412, 413, 414, 416, 418, 419, 420, 421, 422, 423, 424, 425, 426, 427, 428, 429,
    430, 431, 432, 433, 434, 435, 436, 437, 438, 439, 440, 441, 442, 443, 444, 445,
    446, 447, 448, 449, 450, 451, 452, 453, 454, 455, 456, 457, 458, 459, 460, 461,
    462, 463, 464, 465, 466, 467,
};