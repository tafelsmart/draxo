with open('src/modules/waypoints.cpp','w',encoding='utf-8',newline='\n') as f:
    f.write(open('_wp_template.cpp','r',encoding='utf-8').read())
print('OK')
