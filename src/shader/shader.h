/******************************************************************************
    Copyright (C) 2023 by xurei <xureilab@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

class effect_shader {
    public:
    gs_effect_t *effect = nullptr;
    gs_eparam_t *param_tex_a = nullptr;
    gs_eparam_t *param_tex_b = nullptr;
    gs_eparam_t *param_tex_interm = nullptr;
    gs_eparam_t *param_time = nullptr;
    gs_eparam_t *param_upixel = nullptr;
    gs_eparam_t *param_vpixel = nullptr;
    gs_eparam_t *param_rand_seed = nullptr;
    gs_eparam_t *param_current_step = nullptr;
    gs_eparam_t *param_nb_steps = nullptr;

    gs_eparam_t *param_fd_leye_1 = nullptr;
    gs_eparam_t *param_fd_leye_2 = nullptr;
    gs_eparam_t *param_fd_reye_1 = nullptr;
    gs_eparam_t *param_fd_reye_2 = nullptr;
    gs_eparam_t *param_fd_face_1 = nullptr;
    gs_eparam_t *param_fd_face_2 = nullptr;
    gs_eparam_t *param_fd_points_tex = nullptr;

    std::string path;

    ~effect_shader();

    void load(const char *shader_path);

    void release();

    //See cpp file for details
    //private:
    //static bool sourceHasValidBraces(const std::string& str);
    //static std::string removeComments(const std::string& str);
};