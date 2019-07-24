//
// Created by gerardin on 7/19/19.
//

#ifndef NEWEGT_VIEWORVIEWANALYSE_H
#define NEWEGT_VIEWORVIEWANALYSE_H

#include <htgs/api/IData.hpp>
#include <htgs/api/MemoryData.hpp>
#include <FastImage/api/View.h>
#include "ViewAnalyse.h"

namespace egt {

    template <class T>
    class ViewOrViewAnalyse : htgs::IData {

    public:

        explicit ViewOrViewAnalyse(ViewAnalyse *viewAnalyse) : viewAnalyse(viewAnalyse) {}

        explicit ViewOrViewAnalyse(std::shared_ptr<MemoryData<fi::View<T>>> view) : view(view) {}


        std::shared_ptr<MemoryData<fi::View<T>>> getView() {
            return view;
        }

        ViewAnalyse *getViewAnalyse() const {
            return viewAnalyse;
        }

    private:
        ViewAnalyse* viewAnalyse = nullptr;
        std::shared_ptr<MemoryData<fi::View<T>>> view = nullptr;

    };

}

#endif //NEWEGT_VIEWORVIEWANALYSE_H
