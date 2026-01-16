import SwiftUI

struct MainView: View {
    var body: some View {
        ZStack {
            Image("_________L_a_y_e_r___g_r_o_u_p__")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 0.0, height: 0.0)
                .position(x: 0.0, y: 0.0)
            Image("_________L_a_y_e_r___g_r_o_u_p__")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 0.0, height: 0.0)
                .position(x: 0.0, y: 0.0)
            
            
            VStack(alignment: .leading, spacing: 0) {
            }
                .frame(width: 0.0, height: -1.0)
                .position(x: 0.0, y: -0.5)
            Image("____0_0_0_0____1")
                .resizable()
                .aspectRatio(contentMode: .fit)
                .frame(width: 0.0, height: 0.0)
                .position(x: 0.0, y: 0.0)
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
