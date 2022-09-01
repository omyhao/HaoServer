ifeq ($(DEBUG), true)
CXXFLAGS := -g -Wall
Version = debug
else
CXXFLAGS := -g -Wall
Version = release
endif

MyFlags = -std=c++17 -lpthread

# 扫描当前目录下所有.cpp文件
Sources = $(wildcard *.cpp)

# 将字符串中的.cpp代替为.o得到新的字符串
Objects = $(Sources:.cpp=.o)

# 将字符串中的.cpp替换为.d
Dependencies = $(Sources:.cpp=.d)

# 指定Bin文件的位置
Bin := $(addprefix $(Build_Root)/, $(Bin))

# 存放obj文件的目录
Link_Object_Dir = $(Build_Root)/app/link_obj
Dependencies_Dir = $(Build_Root)/app/dep

# 创建上面的两个目录, -p 是递归创建
$(shell mkdir -p $(Link_Object_Dir))
$(shell mkdir -p $(Dependencies_Dir))

# 把目标文件生成到上述目标文件目录去，
Objects := $(addprefix $(Link_Object_Dir)/, $(Objects))
Dependencies := $(addprefix $(Dependencies_Dir)/, $(Dependencies))
Include_Path := $(Include_Path:%=-I%)

# 找到目录中的所有.o文件(编译出来的)
Link_Obj = $(wildcard $(Link_Object_Dir)/*.o)
#因为构建依赖关系时app目录下这个.o文件还没构建出来，所以LINK_OBJ是缺少这个.o的，我们 要把这个.o文件加进来
Link_Obj += $(Objects)

all:$(Dependencies) $(Objects) $(Bin)

ifneq ("$(wildcard $(Dependencies))", "")
include $(Dependencies)
endif

$(Bin):$(Link_Obj)
	@echo "---------- building $(Version) ----------"
	$(CXX) -o $@ $^ $(MyFlags) $(CXXFLAGS)
	@echo "---------- building success. ----------"

$(Link_Object_Dir)/%.o:%.cpp
# 遇到高版本的头文件，这里也必须加$(MyFlags)来指定版本
	@$(CXX) $(MyFlags) $(Include_Path) -o $@ -c $(filter %.cpp, $^)

$(Dependencies_Dir)/%.d:%.cpp
	@echo -n $(Link_Object_Dir)/ > $@
	$(CXX) $(Include_Path) -MM $^ >> $@
	