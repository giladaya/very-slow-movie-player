p_width = 10
p_height = 5.5
p_depth = 7
p_wall_th = 1
p_lip_r = 1


shape = (cq.Workplane("XY")
         .hLine(p_width - p_height/2)
         .tangentArcPoint((0, -p_height))
         .hLine(-(p_width - p_height/2 - p_wall_th))
         .tangentArcPoint((p_wall_th, p_wall_th))
         #.vLine(p_wall_th)
         .hLine((p_width - p_height/2 -  2 * p_wall_th))
         .tangentArcPoint((0, p_height - 2 * p_wall_th))
         .hLine(-(p_width - p_height/2 - p_wall_th))
         .tangentArcPoint((-p_lip_r, p_lip_r))
         #.vLine(p_wall_th)
         .close()
)

debug(shape)

result = shape.extrude(p_depth)

#return the combined result
show_object(result)
cq.exporters.export(result, "cable-holder.stl")
