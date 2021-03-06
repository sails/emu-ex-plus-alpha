#include <imagine/util/math/GLMMat4.hh>
#include <imagine/glm/geometric.hpp>
#include <imagine/glm/gtc/matrix_transform.hpp>
#include <imagine/glm/gtc/matrix_inverse.hpp>

GLMMat4 GLMMat4::translate(GLMVec3 translation) const
{
	return glm::translate(m, translation.v);
}

GLMMat4 GLMMat4::makeTranslate(GLMVec3 translation)
{
	return glm::translate({}, translation.v);
}

GLMMat4 GLMMat4::makePerspectiveFovRH(float fovy, float aspect, float znear, float zfar)
{
	return glm::perspective(fovy, aspect, znear, zfar);
}

GLMMat4 GLMMat4::scale(GLMVec3 factors) const
{
	return glm::scale(m, factors.v);
}

GLMMat4 GLMMat4::rotate(float angle, GLMVec3 axis) const
{
	return glm::rotate(m, angle, axis.v);
}

GLMMat4 GLMMat4::invert() const
{
	return glm::inverse(m);
}

GLMVec3 GLMMat4::project(IG::Rect2<int> viewport, GLMVec3 obj) const
{
	glm::ivec4 viewportVec {viewport.x, viewport.y, viewport.x2, viewport.y2};
	return glm::project(obj.v, {}, m, viewportVec);
}

GLMVec3 GLMMat4::unproject(IG::Rect2<int> viewport, GLMVec3 win, GLMMat4 inverse) const
{
	glm::ivec4 viewportVec {viewport.x, viewport.y, viewport.x2, viewport.y2};
	return glm::unProjectWithInverse(win.v, inverse.m, viewportVec);
}
