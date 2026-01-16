import SwiftUI

struct MainView: View {
    var body: some View {
        ZStack {
            Image("_________L_a_y_e_r___g_r_o_u_p__")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 0.0, height: 0.0)
                .position(x: 0.0, y: 0.0)
            Image("____T_T_R_u_P___")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 64.0, height: 46.0)
                .position(x: 72.0, y: 103.0)
            Image("_____q_t_q_u_i_c_k__")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 36.0, height: 48.0)
                .position(x: 76.0, y: 40.0)
            Image("____0_0_")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 64.0, height: 46.0)
                .position(x: 72.0, y: 173.0)
            Image("_____s_l_i_n_t__")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 36.0, height: 48.0)
                .position(x: 168.0, y: 103.0)
            Image("_____s_l_i_n_t__0_0_0_0_S___")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 36.0, height: 48.0)
                .position(x: 168.0, y: 173.0)
            Image("_____f_l_u_t_t_e_r__")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 32.0, height: 40.0)
                .position(x: 256.0, y: 102.0)
            Image("_____f_l_u_t_t_e_r___p_i_x_e_l_e_d__")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 32.0, height: 40.0)
                .position(x: 256.0, y: 172.0)
            Image("____0_0_0_0_0_0____1")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 0.0, height: 0.0)
                .position(x: 0.0, y: 0.0)
        }
        .frame(width: 320.0, height: 240.0)
    }
}

#Preview {
    MainView()
}
